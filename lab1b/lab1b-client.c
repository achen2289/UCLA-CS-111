// NAME: Alex Chen
// EMAIL: achen2289@gmail.com
// ID: 005299047

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netdb.h>
#include <getopt.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <poll.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <zlib.h>

#define KILL '\003'
#define EOT '\004'

int sock_fd = -1, log_fd = -1, compress_flag = 0;
struct termios saved_attributes; // initial terminal attributes
z_stream to_server, from_server;

const char correct_usage[] = "./lab1b-client --port=PORT [--log=filename] [--compress]";
const char missing_port_error[] = "ERROR: port argument is missing";
const char reset_terminal_error[] = "ERROR: Terminal modes could not be restored prior to exit";
const char tattr_retrieval_error[] = "ERROR: Terminal modes could not be retrieved";
const char tattr_set_error[] = "ERROR: Terminal modes could not be modified";
const char tattr_reset_error[] = "ERROR: Terminal modes could not be restored";
const char socket_creation_error[] = "ERROR: Socket could not be created";
const char poll_error[] = "ERROR: There was a polling error";
const char poll_stdin_error[] = "ERROR: There was an error reading from stdin";
const char file_open_error[] = "ERROR: The log file could not be opened";
const char sprintf_error[] = "ERROR: sprintf could not be used to write to log file";
const char compression_error[] = "ERROR: Compression could not occur";
const char decompression_error[] = "ERROR: Decompression could not occur";

// Check for error, and reset terminal modes before exiting
void check_return_error(int status, const char *msg, int exit_status) {
    if (status < 0) {
        fprintf(stderr, "%s\n", msg);
        exit(exit_status);
    }
}

// Parse all options and save in function variables
void get_options(int argc, char *argv[], char **port, char **log_file) {
    int c;
    const char optstring[] = ":p:l:c";
    struct option longopts[] = {
        {"port", required_argument, NULL, 'p'},
        {"log", required_argument, NULL, 'l'},
        {"compress", no_argument, &compress_flag, 1},
        {0, 0, 0, 0}
    };
    while ((c = getopt_long(argc, argv, optstring, longopts, NULL)) != -1) {
        switch (c) {
            case 'p': // input option found
                *port = optarg;
                break;
            case 'l':
                *log_file = optarg;
                break;
            case ':': // missing option argument
                fprintf(stderr, "%s: option '-%c' requires an argument\n", argv[0], optopt);
                exit(1);
            case '?': // invalid option
                fprintf(stderr, "%s: option '-%c' is invalid: ignored\ncorrect usage (shell option is optional): %s\n", \
                        argv[0], optopt, correct_usage);
                exit(1);
            default:
                break;
        }
    }
}

// Reset terminal attributes and exit
void reset_terminal_modes() {
    if (sock_fd != -1) {
        shutdown(sock_fd, SHUT_RDWR);
    }
    if (log_fd != -1) {
        close(log_fd);
    }
    int ret = tcsetattr(STDIN_FILENO, TCSANOW, &saved_attributes);
    check_return_error(ret, reset_terminal_error, 1);
}

// Set terminal to non-canonical, no echo mode
void set_terminal_modes() {
    struct termios t_attributes;
    check_return_error(tcgetattr(STDIN_FILENO, &saved_attributes), tattr_retrieval_error, 1);
    check_return_error(tcgetattr(STDIN_FILENO, &t_attributes), tattr_retrieval_error, 1);
    t_attributes.c_iflag = ISTRIP;
    t_attributes.c_oflag = 0;
    t_attributes.c_lflag = 0;
    check_return_error(tcsetattr(STDIN_FILENO, TCSANOW, &t_attributes), tattr_set_error, 1);
    atexit(reset_terminal_modes);
}

// Connect client to server (localhost)
void connect_client(char *hostname, unsigned int port) {
    struct sockaddr_in serv_addr;
    struct hostent *server;

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    check_return_error(sock_fd, socket_creation_error, 1);

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    server = gethostbyname(hostname);
    memcpy((char *) &serv_addr.sin_addr.s_addr, (char *) server->h_addr, server->h_length);
    memset(serv_addr.sin_zero, '\0', sizeof(serv_addr.sin_zero));

    // Connect to serv_addr through *sock_fd
    int ret = connect(sock_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
    check_return_error(ret, strerror(errno), 1);
}

// Read input from both keyboard and socket
// Return 1 if socket returns EOF, else return 0
int read_from_source(char source) {
    const int buf_size = 256;
    int bytes_read, bytes_written;
    char buffer[buf_size];
    char map_cr_lf[] = "\r\n", lf = '\n';

    if (source == 'k') {
        bytes_read = read(STDIN_FILENO, buffer, buf_size);
    }
    else {
        bytes_read = read(sock_fd, buffer, buf_size);
        if (bytes_read == 0) {
            return 1;
        }
    }
    check_return_error(bytes_read, strerror(errno), 1);

    // Write input from keyboard to stdout
    if (source == 'k') {
        // Write to stdout
        for (int i=0; i<bytes_read; i++) {
            if (buffer[i] == '\r' || buffer[i] == '\n')
            {
                bytes_written = write(STDOUT_FILENO, map_cr_lf, 2);
                check_return_error(bytes_written, strerror(errno), 1);
            }
            else {
                bytes_written = write(STDOUT_FILENO, &buffer[i], 1);
                check_return_error(bytes_written, strerror(errno), 1);
            }
        }
        // Compress if applicable then write to socket
        if (compress_flag) {
            char compression_buffer[buf_size];
            to_server.avail_in = bytes_read;
            to_server.next_in = (unsigned char *) buffer;
            to_server.avail_out = buf_size;
            to_server.next_out = (unsigned char *) compression_buffer;

            while (to_server.avail_in > 0) {
                deflate(&to_server, Z_SYNC_FLUSH);
            }

            bytes_written = write(sock_fd, compression_buffer, buf_size - to_server.avail_out);
            check_return_error(bytes_written, compression_error, 1);

            if (log_fd != -1) {
                char sent_string[30];
                check_return_error(sprintf(sent_string, "SENT %d bytes: ", buf_size - to_server.avail_out), sprintf_error, 1);
                bytes_written = write(log_fd, sent_string, strlen(sent_string));
                check_return_error(bytes_written, strerror(errno), 1);

                bytes_written = write(log_fd, compression_buffer, buf_size - to_server.avail_out);
                check_return_error(bytes_written, strerror(errno), 1);

                bytes_written = write(log_fd, &lf, 1);
                check_return_error(bytes_written, strerror(errno), 1);
            }
        }
        else {
            bytes_written = write(sock_fd, buffer, bytes_read);
            check_return_error(bytes_written, strerror(errno), 1);

            if (log_fd != -1) {
                char sent_string[30];
                check_return_error(sprintf(sent_string, "SENT %d bytes: ", bytes_read), sprintf_error, 1);
                bytes_written = write(log_fd, sent_string, strlen(sent_string));
                check_return_error(bytes_written, strerror(errno), 1);

                bytes_written = write(log_fd, buffer, bytes_read);
                check_return_error(bytes_written, strerror(errno), 1);

                bytes_written = write(log_fd, &lf, 1);
                check_return_error(bytes_written, strerror(errno), 1);
            }
        }
    }

    // Write input from socket to stdout, no processing required
    if (source == 's') {
        if (compress_flag) {
            char decompression_buffer[buf_size];
            from_server.avail_in = bytes_read;
            from_server.next_in = (unsigned char *) buffer;
            from_server.avail_out = buf_size;
            from_server.next_out = (unsigned char *) decompression_buffer;

            while (from_server.avail_in > 0) {
                inflate(&from_server, Z_SYNC_FLUSH);
            }

            bytes_written = write(STDOUT_FILENO, decompression_buffer, buf_size - from_server.avail_out);
            check_return_error(bytes_written, compression_error, 1);
        }
        else {
            bytes_written = write(STDOUT_FILENO, buffer, bytes_read);
            check_return_error(bytes_written, strerror(errno), 1);
        }

        if (log_fd != -1) {
            char received_string[34];
            check_return_error(sprintf(received_string, "RECEIVED %d bytes: ", bytes_read), sprintf_error, 1);
            bytes_written = write(log_fd, received_string, strlen(received_string));
            check_return_error(bytes_written, strerror(errno), 1);

            bytes_written = write(log_fd, buffer, bytes_read);
            check_return_error(bytes_written, strerror(errno), 1);

            bytes_written = write(log_fd, &lf, 1);
            check_return_error(bytes_written, strerror(errno), 1);
        }
    }
    return 0;
}

// Talk with server
void talk() {
    struct pollfd pollfds[2];
    pollfds[0].fd = 0;
    pollfds[0].events = POLLIN | POLLHUP | POLLERR;
    pollfds[0].revents = 0;
    pollfds[1].fd = sock_fd;
    pollfds[1].events = POLLIN | POLLHUP | POLLERR;
    pollfds[1].revents = 0;

    while (420) { // poll stdin and socket for input
        check_return_error(poll(pollfds, 2, 0), poll_error, 1);

        if (pollfds[0].revents & POLLIN) {
            if (read_from_source('k')) {
                break;
            }
        }
        if (pollfds[1].revents & POLLIN) {
            if (read_from_source('s')) {
                break;
            }
        }
        if (pollfds[0].revents & (POLLHUP | POLLERR)) {
            fprintf(stderr, poll_stdin_error);
            exit(1);
        }
        if (pollfds[1].revents & (POLLHUP | POLLERR)) {
            exit(0);
        }
    }
}

// Initialize compression streams
void initialize_compression() {
    if (compress_flag) {
        to_server.zalloc = Z_NULL;
        to_server.zfree = Z_NULL;
        to_server.opaque = Z_NULL;
        if (deflateInit(&to_server, Z_DEFAULT_COMPRESSION) != Z_OK) {
            fprintf(stderr, compression_error);
            exit(1);
        }

        from_server.zalloc = Z_NULL;
        from_server.zfree = Z_NULL;
        from_server.opaque = Z_NULL;
        if (inflateInit(&from_server) != Z_OK) {
            fprintf(stderr, decompression_error);
            exit(1);
        }
    }
}

// Close compression streams
void end_compression() {
    if (compress_flag) {
        deflateEnd(&to_server);
        inflateEnd(&from_server);
    }
}

int main(int argc, char **argv) {
    char *port = '\0', *log_file = '\0', *hostname = "localhost";
    get_options(argc, argv, &port, &log_file);
    if (port == NULL) {
        check_return_error(-1, missing_port_error, 1);
    }
    set_terminal_modes();
    int port_num = atoi(port);
    if (log_file != NULL) {
        log_fd = open(log_file, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU);
        check_return_error(log_fd, file_open_error, 1);
    }
    initialize_compression();
    connect_client(hostname, port_num);
    talk();
    end_compression();
    exit(0);
}