// NAME: Alex Chen
// EMAIL: achen2289@gmail.com
// ID: 005299047

#include <mraa.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <getopt.h>
#include <math.h>
#include <sys/socket.h>
#include <netdb.h>

#define B 4275 // thermistor value
#define R0 100000.0 // nominal base value
#define A0 1

const char correct_usage[] = "./lab4c_tcp [--period=#] [--scale=(C or F)] [--log=] --id=[] --host=[] portnumber";
const char incorrect_ports_error[] = "ERROR: Program accepts a single port number";
const char socket_creation_error[] = "ERROR: Socket could not be created";
const char missing_argument_error[] = "ERROR: Mandatory arguments are missing";
const char initialize_sensor_error[] = "ERROR: Sensor(s) could not be initialized";
const char invalid_period_error[] = "ERROR: Invalid period was provided";

int period = 1, log_fd = -1, generate_reports = 1, port = -1, sock_fd = -1;
char scale = 'F', *log_file = '\0', *host = '\0', *id = '\0';
mraa_aio_context temp_sensor;

struct timeval curr_time, last_read;
struct tm *local_time;

// Check for error, and reset terminal modes before exiting
void check_return_error(int status, const char *msg, int exit_status) {
    if (status < 0) {
        fprintf(stderr, "%s\n", msg);
        exit(exit_status);
    }
}

// Print SHUTDOWN record upon receving shutdown command
void shut_down() {
    check_return_error(gettimeofday(&curr_time, NULL), strerror(errno), 1);
    local_time = localtime(&curr_time.tv_sec);
    if (local_time == NULL) {
        check_return_error(-1, strerror(errno), 1);
    }
    char report[256] = {0};
    sprintf(report, "%02d:%02d:%02d %s\n", local_time->tm_hour, local_time->tm_min, local_time->tm_sec, "SHUTDOWN");
    
    printf("%s", report);
    fflush(stdout);

    int bytes_written;

    if (log_fd != -1) {
        bytes_written = write(log_fd, report, strlen(report));
        check_return_error(bytes_written, strerror(errno), 1);
    }
    bytes_written = write(sock_fd, report, strlen(report));
    check_return_error(bytes_written, strerror(errno), 1);

    exit(0);
}

// Close input sensors and log file descriptor upon exit
void exit_handler() {
    mraa_aio_close(temp_sensor);
    if (log_fd > 0) {
        close(log_fd);
    }
    close(sock_fd);
}

// Parse all options and save in function variables
void get_options(int argc, char *argv[]) {
    int c;
    const char optstring[] = ":p:s:l:i:h:";
    struct option longopts[] = {
        {"period", required_argument, NULL, 'p'},
        {"scale", required_argument, NULL, 's'},
        {"log", required_argument, NULL, 'l'},
        {"id", required_argument, NULL, 'i'},
        {"host", required_argument, NULL, 'h'},
        {0, 0, 0, 0}
    };
    while ((c = getopt_long(argc, argv, optstring, longopts, NULL)) != -1) {
        switch (c) {
            case 'p':
                period = atoi(optarg);
                break;
            case 's':
                scale = *optarg;
                if (!(scale == 'F' || scale == 'C') || strlen(optarg) > 1) {
                    fprintf(stderr, "%s: option '-%c' is invalid", argv[0], optopt);
                    exit(1);
                }
                break;
            case 'l':
                log_file = optarg;
                log_fd = open(log_file, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
                check_return_error(log_fd, strerror(errno), 1);
                break;
            case 'i':
                id = optarg;
                break;
            case 'h':
                host = optarg;
                break;
            case ':': // missing option argument
                fprintf(stderr, "%s: option '-%c' requires an argument\n", argv[0], optopt);
                exit(1);
            case '?': // invalid option
                fprintf(stderr, "%s: option '-%c' is invalid: ignored\ncorrect usage: %s\n", \
                        argv[0], optopt, correct_usage);
                exit(1);
            default:
                break;
        }
    }
    
    int i;
    for (i=optind; i<argc; i++) {
        if (port != -1) {
            check_return_error(-1, incorrect_ports_error, 1);
            exit(1);
        }
        port = atoi(argv[i]);
    }
    if (port < 0) {
        check_return_error(-1, incorrect_ports_error, 1);
    }

    if (host == NULL) {
        check_return_error(-1, missing_argument_error, 1);
    }
    check_return_error(log_fd, missing_argument_error, 1);
    if (id == NULL) {
        check_return_error(-1, missing_argument_error, 1);
    }
    check_return_error(port, missing_argument_error, 1);
}

// Connect client to server (localhost)
void connect_client(char *hostname, unsigned int port) {
    struct sockaddr_in serv_addr;
    struct hostent *server;

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    check_return_error(sock_fd, socket_creation_error, 2);

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    server = gethostbyname(hostname);
    memcpy((char *) &serv_addr.sin_addr.s_addr, (char *) server->h_addr, server->h_length);
    memset(serv_addr.sin_zero, '\0', sizeof(serv_addr.sin_zero));

    // Connect to serv_addr through *sock_fd
    int ret = connect(sock_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
    check_return_error(ret, strerror(errno), 2);
}

// Send ID to server, and log
void send_ID() {
    int bytes_written;

    char id_str_record[13] = {0};
    sprintf(id_str_record, "ID=%s\n", id);
    
    bytes_written = write(log_fd, id_str_record, strlen(id_str_record));
    check_return_error(bytes_written, strerror(errno), 1);

    bytes_written = write(sock_fd, id_str_record, strlen(id_str_record));
    check_return_error(bytes_written, strerror(errno), 1);
}

// Initialize temperature sensor and button
// Detect button press on rising edge
void initialize_sensors() {
    temp_sensor = mraa_aio_init(A0);
    if (temp_sensor == NULL) {
        mraa_deinit();
        check_return_error(-1, initialize_sensor_error, 2);
    }
}

// Convert input sensor reading to actual temperature in C or F
float convert_temp_reading(float reading) {
    float R = 1023.0 / reading - 1.0;
    R *= R0;
    float C = 1.0 / (log(R/R0)/B + 1/298.15) - 273.15;
    float F = (9 * C)/5 + 32;
    return scale == 'C' ? C : F;
}

// Read and convert temperature
float read_temperature() {
    float temp_reading = mraa_aio_read(temp_sensor);
    float temp = convert_temp_reading(temp_reading);
    return temp;
}

// Print temperature record reading
void print_record() {
    float temp_reading = read_temperature();
    local_time = localtime(&curr_time.tv_sec);
    if (local_time == NULL) {
        check_return_error(-1, strerror(errno), 1);
    }
    char report[256] = {0};
    sprintf(report, "%02d:%02d:%02d %.1f\n", local_time->tm_hour, local_time->tm_min, local_time->tm_sec, temp_reading);

    printf("%s", report);
    fflush(stdout);

    int bytes_written = write(sock_fd, report, strlen(report));
    check_return_error(bytes_written, strerror(errno), 1);

    if (log_fd != -1) {
        bytes_written = write(log_fd, report, strlen(report));
        check_return_error(bytes_written, strerror(errno), 1);
    }
}

// Execute commands provided from server
// Split input by newlines
// Log all commands to log file if existing
void exec_commands(char *buffer) {
    char *token = strtok(buffer, "\n");
    int bytes_written;
    while (token) {
        while (token && (*token == ' ' || *token == '\t')) {
            token++;
        }
        if (log_fd != -1) {
            bytes_written = write(log_fd, token, strlen(token));
            check_return_error(bytes_written, strerror(errno), 1);
            bytes_written = write(log_fd, "\n", 1);
            check_return_error(bytes_written, strerror(errno), 1);
        }
        if (strncmp(token, "SCALE=F", 7) == 0) {
            scale = 'F';
        }
        else if (strncmp(token, "SCALE=C", 7) == 0) {
            scale = 'C';
        }
        else if (strncmp(token, "PERIOD=", 7) == 0) {
            period = atoi(token + 7);
            if (period == 0) {
                check_return_error(-1, invalid_period_error, 1);
            }
        }
        else if (strncmp(token, "STOP", 4) == 0) {
            generate_reports = 0;
        }
        else if (strncmp(token, "START", 5) == 0) {
            generate_reports = 1;
        }
        else if (strncmp(token, "OFF", 3) == 0) {
            shut_down();
        }
        token = strtok(NULL, "\n");
    }
}

// Read input from temp sensor and server
// Poll for input from server
void read_input() {
    struct pollfd pollfds[1];
    pollfds[0].fd = sock_fd;
    pollfds[0].events = POLLIN | POLLHUP | POLLERR;
    pollfds[0].revents = 0;

    // First read, regardless of period
    check_return_error(gettimeofday(&curr_time, NULL), strerror(errno), 1);
    print_record();
    check_return_error(gettimeofday(&last_read, NULL), strerror(errno), 1);

    while (42069) {
        check_return_error(gettimeofday(&curr_time, NULL), strerror(errno), 1);
        if ((curr_time.tv_sec >= last_read.tv_sec + period) && generate_reports) {
            print_record();
            check_return_error(gettimeofday(&last_read, NULL), strerror(errno), 1);
        }
        int polls = poll(pollfds, 1, 1000);
        check_return_error(polls, strerror(errno), 1);
        if (polls > 0 && (pollfds[0].revents & POLLIN)) {
            char buffer[1024] = {0};
            int bytes_read = read(sock_fd, buffer, 1024);
            check_return_error(bytes_read, strerror(errno), 1);
            exec_commands(buffer);
        }
    }
}

int main(int argc, char **argv) {
    get_options(argc, argv);
    connect_client(host, port);

    send_ID();

    initialize_sensors();
    atexit(exit_handler);

    read_input();
    exit(0);
}