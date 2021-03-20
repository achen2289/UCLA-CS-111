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
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <poll.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <unistd.h>
#include <zlib.h>

#define KILL '\003'
#define EOT '\004'

int to_shell[2], from_shell[2], rc, compress_flag = 0, sock_fd = -1, temp_sock_fd;
z_stream to_client, from_client;
char shell_prog[] = "/bin/bash";

const char correct_usage[] = "./lab1b-server --port=PORT [--compress]";
const char close_to_shell_error[] = "ERROR: An end of the pipe to shell could not be closed";
const char close_from_shell_error[] = "ERROR: An end of the pipe from shell could not be closed";
const char fork_error[] = "ERROR: Terminal process could not be forked";
const char kill_error[] = "ERROR: Process could not be killed";
const char wait_error[] = "ERROR: Termination of the child shell cannot be waited for";
const char poll_error[] = "ERROR: The socket and pipes could not be polled";
const char poll_stdin_error[] = "ERROR: There was an error reading from the client";
const char missing_port_error[] = "ERROR: port argument is missing";
const char dup_error[] = "ERROR: File descriptors could not be duplicated";
const char run_shell_error[] = "ERROR: The specified shell could not be executed";
const char compression_error[] = "ERROR: Compression could not occur";
const char decompression_error[] = "ERROR: Decompression could not occur";
const char socket_creation_error[] = "ERROR: Socket could not be created";

// Check for error, and reset terminal modes before exiting
void check_return_error(int status, const char *msg, int exit_status) {
    if (status == -1) {
        fprintf(stderr, "%s\n", msg);
        exit(exit_status);
    }
}

// Parse all options and save in function variables
void get_options(int argc, char *argv[], char **port) {
    int c;
    const char optstring[] = ":p:c";
    struct option longopts[] = {
        {"port", required_argument, NULL, 'p'},
        {"compress", no_argument, &compress_flag, 1},
        {0, 0, 0, 0}
    };
    while ((c = getopt_long(argc, argv, optstring, longopts, NULL)) != -1) {
        switch (c) {
            case 'p': // input option found
                *port = optarg;
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

// Wait for child to exit and get exit status
void harvest_shell_exit() {
    int wstatus;
    // check_return_error(waitpid(rc, &wstatus, 0), wait_error, 1);
    waitpid(rc, &wstatus, 0);
    fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", WTERMSIG(wstatus), WEXITSTATUS(wstatus));
}

// Handles SIGPIPE error
void sigpipe_handler(int signum) {
    if (signum == SIGPIPE) {
        fprintf(stderr, "SIGPIPE error: shell has exited");
        close(to_shell[1]);
        close(from_shell[0]);
        // check_return_error(close(to_shell[1]), close_to_shell_error, 1);
        // check_return_error(close(from_shell[0]), close_from_shell_error, 1);
        harvest_shell_exit();
        exit(0);
    }
}

// Connect client to server
void connect_server(unsigned int port) {
    struct sockaddr_in my_addr, their_addr;
    unsigned int sin_size;

    temp_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    check_return_error(temp_sock_fd, socket_creation_error, 1);

    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(port);
    my_addr.sin_addr.s_addr = INADDR_ANY;

    memset(my_addr.sin_zero, '\0', sizeof(my_addr.sin_zero));

    // Bind address to created socket
    int ret = bind(temp_sock_fd, (struct sockaddr *) &my_addr, sizeof(my_addr));
    check_return_error(ret, strerror(errno), -1);

    // Listen for incoming connections on socket
    ret = listen(temp_sock_fd, 8);

    // Accept connection from client and communicate using *new_fd
    sin_size = sizeof(their_addr);
    sock_fd = accept(temp_sock_fd, (struct sockaddr *) &their_addr, (socklen_t *) &sin_size);
    check_return_error(sock_fd, strerror(errno), -1);

    // shutdown(temp_sock_fd, SHUT_RDWR);
}

// Read from keyboard or shell, depending on source
int read_from_source(char source) {
    const int buf_size = 256;
    int bytes_read, bytes_written, EOT_received = 0;
    char buffer[buf_size], lf = '\n', map_cr_lf[] = "\r\n";

    if (source == 'c') {
        bytes_read = read(sock_fd, buffer, buf_size);
    }
    else {
        bytes_read = read(from_shell[0], buffer, buf_size); 
    }
    check_return_error(bytes_read, strerror(errno), 1);
    
    if (source == 'c') {
        if (compress_flag) {
            char decompression_buffer[buf_size*4];
            from_client.avail_in = bytes_read;
            from_client.next_in = (unsigned char *) buffer;
            from_client.avail_out = buf_size*4;
            from_client.next_out = (unsigned char *) decompression_buffer;

            while (from_client.avail_in > 0) {
                inflate(&from_client, Z_SYNC_FLUSH);
            }
            for (int i=0; (unsigned int) i<buf_size*4 - from_client.avail_out; i++) {
                if (decompression_buffer[i] == KILL) {
                    check_return_error(kill(rc, SIGINT), kill_error, 1);
                }
                else if (decompression_buffer[i] == EOT) {
                    EOT_received = 1;
                }
                else if (decompression_buffer[i] == '\r' || decompression_buffer[i] == '\n') {
                    bytes_written = write(to_shell[1], &lf, 1);
                    check_return_error(bytes_written, strerror(errno), 1);
                }
                else {
                    bytes_written = write(to_shell[1], &decompression_buffer[i], 1);
                    check_return_error(bytes_written, strerror(errno), 1);
                }
            }
        }
        else {
            for (int i=0; i<bytes_read; i++) {
                if (buffer[i] == KILL) {
                    check_return_error(kill(rc, SIGINT), kill_error, 1);
                }
                else if (buffer[i] == EOT) {
                    EOT_received = 1;
                }
                else if (buffer[i] == '\r' || buffer[i] == '\n') {
                    bytes_written = write(to_shell[1], &lf, 1);
                    check_return_error(bytes_written, strerror(errno), 1);
                }
                else {
                    bytes_written = write(to_shell[1], &buffer[i], 1);
                    check_return_error(bytes_written, strerror(errno), 1);
                }
            }
        }  
    }

    if (source == 's') {
        int count = 0;
        int j = 0;
        for (int i = 0; i < bytes_read; i++) {
            if (buffer[i] == EOT) { //EOF from shell
                EOT_received = 1;
            } 
            else if (buffer[i] == '\n') {
                if (compress_flag) {
                    char compression_buffer[256];
                    to_client.avail_in = count;
                    to_client.next_in = (unsigned char *) (buffer + j);
                    to_client.avail_out = 256;
                    to_client.next_out = (unsigned char *) compression_buffer;

                    while (to_client.avail_in > 0) {
                        deflate(&to_client, Z_SYNC_FLUSH);
                    }
                    
                    bytes_written = write(sock_fd, compression_buffer, 256 - to_client.avail_out);
                    check_return_error(bytes_written, strerror(errno), 1);

                    char compression_buffer2[256];
                    char temp[2] = {'\r', '\n'};
                    to_client.avail_in = 2;
                    to_client.next_in = (unsigned char *) temp;
                    to_client.avail_out = 256;
                    to_client.next_out = (unsigned char *) compression_buffer2;

                    while (to_client.avail_in > 0) {
                        deflate(&to_client, Z_SYNC_FLUSH);
                    }
                    bytes_written = write(sock_fd, compression_buffer2, 256 - to_client.avail_out);
                    check_return_error(bytes_written, strerror(errno), 1);
                } 
                else {
                    bytes_written = write(sock_fd, buffer + j, count);
                    check_return_error(bytes_written, strerror(errno), 1);

                    bytes_written = write(sock_fd, map_cr_lf, 2);
                    check_return_error(bytes_written, strerror(errno), 1);
                }
                j += count + 1;
                count = 0;
                continue;
            }
            count++;
        }
        bytes_written = write(sock_fd, buffer+j, count);
        check_return_error(bytes_written, strerror(errno), 1);
    }
    return EOT_received;
}

// Fork process and execute shell program on child
// Create two pipes between the parent and child processes
void create_child_and_talk() {
    check_return_error(pipe(to_shell), strerror(errno), 1);
    check_return_error(pipe(from_shell), strerror(errno), 1);

    signal(SIGPIPE, sigpipe_handler);

    rc = fork();
    check_return_error(rc, fork_error, 1);

    if (rc == 0) { // child process
        check_return_error(close(from_shell[0]), close_from_shell_error, 1); // close read end of pipe from child
        check_return_error(close(to_shell[1]), close_to_shell_error, 1); // close write end of pipe to child

        // redirect read end of to_shell to child's stdin
        check_return_error(dup2(to_shell[0], STDIN_FILENO), dup_error, 1);
        check_return_error(close(to_shell[0]), close_to_shell_error, 1);

        // redirect child's stdout to write end of from_shell
        check_return_error(dup2(from_shell[1], STDOUT_FILENO), dup_error, 1);

        // redirect child's stderr to write end of from_shell
        check_return_error(dup2(from_shell[1], STDERR_FILENO), dup_error, 1);

        check_return_error(close(from_shell[1]), close_from_shell_error, 1); // close extra from_shell write end

        char *shell_args[2] = {shell_prog, NULL};
        check_return_error(execv(shell_prog, shell_args), run_shell_error, 1);
    }
    else { // parent process, has own copy of file descriptors
        close(from_shell[1]); // close write end of pipe to parent
        close(to_shell[0]); // close read end of pipe from parent

        // Set up polling to read data from keyboard and from shell
        // POLLIN: data to be read
        // POLLUP: reading from pipe whose write end has been closed
        // POLLERR: writing to a pipe whose read end has been closed
        struct pollfd pollfds[2];
        pollfds[0].fd = sock_fd;
        pollfds[0].events = POLLIN | POLLHUP | POLLERR;
        pollfds[0].revents = 0;
        pollfds[1].fd = from_shell[0];
        pollfds[1].events = POLLIN | POLLHUP | POLLERR;
        pollfds[1].revents = 0;

        int EOT_received = 0, wstatus;

        while (78) {
            if (waitpid(rc, &wstatus, WNOHANG) != 0) {
                break;
			}
            check_return_error(poll(pollfds, 2, 0), poll_error, 1);
            if (pollfds[0].revents & POLLIN) {
                if (read_from_source('c') == 1) { // from client
                    EOT_received = 1;
                }
            }
            else if (pollfds[0].revents & (POLLHUP | POLLERR)) {
                fprintf(stderr, poll_stdin_error);
                exit(1);
            }
            if (pollfds[1].revents & POLLIN) {
                if (read_from_source('s') == 1) { // from shell
                    EOT_received = 1;
                }
            }
            else if (pollfds[1].revents & (POLLHUP | POLLERR)) {
                break;
            }
            if (EOT_received) {
                break;
            }
        }

        // Close remaining open pipes of parent
        check_return_error(close(to_shell[1]), close_to_shell_error, 1);
        check_return_error(close(from_shell[0]), close_from_shell_error, 1);
        
        harvest_shell_exit();
        
        shutdown(sock_fd, SHUT_RDWR);
        shutdown(temp_sock_fd, SHUT_RDWR);
    }
}

// Initialize compression streams
void initialize_compression() {
    if (compress_flag) {
        to_client.zalloc = Z_NULL;
        to_client.zfree = Z_NULL;
        to_client.opaque = Z_NULL;
        if (deflateInit(&to_client, Z_DEFAULT_COMPRESSION) != Z_OK) {
            fprintf(stderr, compression_error);
            exit(1);
        }

        from_client.zalloc = Z_NULL;
        from_client.zfree = Z_NULL;
        from_client.opaque = Z_NULL;
        if (inflateInit(&from_client) != Z_OK) {
            fprintf(stderr, decompression_error);
            exit(1);
        }
    }
}

// CLose compression streams
void end_compression() {
    if (compress_flag) {
        deflateEnd(&to_client);
        inflateEnd(&from_client);
    }
}

int main(int argc, char **argv) {
    char *port = '\0';
    get_options(argc, argv, &port);
    if (port == NULL) {
        check_return_error(-1, missing_port_error, 1);
    }
    int port_num = atoi(port);
    initialize_compression();
    connect_server(port_num);
    create_child_and_talk();
    end_compression();
    exit(0);
}
