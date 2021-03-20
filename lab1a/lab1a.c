// NAME: Alex Chen
// EMAIL: achen2289@gmail.com
// ID: 005299047

#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <poll.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#define KILL '\003'
#define EOT '\004'

int sigpipe_status = 0;
int to_shell[2], from_shell[2];
int rc;
char shell_prog[] = "/bin/bash";

struct termios saved_attributes; // initial terminal attributes

const char correct_usage[] = "./lab1a --shell=[PATH]";
const char tattr_retrieval_error[] = "ERROR: Terminal modes could not be retrieved";
const char tattr_set_error[] = "ERROR: Terminal modes could not be modified";
const char tattr_reset_error[] = "ERROR: Terminal modes could not be restored";
const char fork_error[] = "ERROR: Terminal process could not be forked";
const char dup_error[] = "ERROR: File descriptors could not be duplicated";
const char run_shell_error[] = "ERROR: The specified shell could not be executed";
const char poll_error[] = "ERROR: There was a polling error";
const char poll_stdin_error[] = "ERROR: There was an error reading from stdin";
const char kill_error[] = "ERROR: Process could not be killed";
const char close_to_shell_error[] = "ERROR: An end of the pipe to shell could not be closed";
const char close_from_shell_error[] = "ERROR: An end of the pipe from shell could not be closed";
const char wait_error[] = "ERROR: Termination of the child shell cannot be waited for";
const char signal_error[] = "ERROR: The signal handler could not be registered";

// Check for error, and reset terminal modes before exiting
void check_return_error(int status, const char *msg, int exit_status) {
    if (status == -1) {
        fprintf(stderr, "%s\n", msg);
        exit(exit_status);
    }
}

// Parse all options and save in function variables
void get_options(int argc, char *argv[], int *shell) {
    int c;
    const char optstring[] = ":s:";
    struct option longopts[] = {
        {"shell", no_argument, NULL, 's'},
        {0, 0, 0, 0}
    };
    while ((c = getopt_long(argc, argv, optstring, longopts, NULL)) != -1) {
        switch (c) {
            case 's': // input option found
                *shell = 1;
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

// Set terminal to non-canonical, no echo mode
void set_terminal_modes() {
    struct termios t_attributes;
    check_return_error(tcgetattr(STDIN_FILENO, &saved_attributes), tattr_retrieval_error, 1);
    check_return_error(tcgetattr(STDIN_FILENO, &t_attributes), tattr_retrieval_error, 1);
    t_attributes.c_iflag = ISTRIP;
    t_attributes.c_oflag = 0;
    t_attributes.c_lflag = 0;
    check_return_error(tcsetattr(STDIN_FILENO, TCSANOW, &t_attributes), tattr_set_error, 1);
}

// Wait for child to exit and get exit status
void harvest_shell_exit() {
    int wstatus;
    check_return_error(waitpid(rc, &wstatus, 0), wait_error, 1);
    fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", WTERMSIG(wstatus), WEXITSTATUS(wstatus));
}

// Handles SIGPIPE error
void sigpipe_handler(int signum) {
    if (signum == SIGPIPE) {
        fprintf(stderr, "SIGPIPE error: shell has exited");
        check_return_error(close(to_shell[1]), close_to_shell_error, 1);
        check_return_error(close(from_shell[0]), close_from_shell_error, 1);
        harvest_shell_exit();
        exit(0);
    }
}

// Read from keyboard or shell, depending on source
int read_from_keyboard_shell(char source) {
    const int buf_size = 256;
    int bytes_read, bytes_written, EOT_received = 0;
    char buffer[buf_size];
    char map_cr_lf[] = "\r\n", eot_repr[] = "^D", kill_repr[] = "^C";
    char lf = '\n';

    if (source == 'k') {
        bytes_read = read(STDIN_FILENO, buffer, buf_size);
    }
    else {
        bytes_read = read(from_shell[0], buffer, buf_size);
    }
    check_return_error(bytes_read, strerror(errno), 1);

    for (int i=0; i<bytes_read; i++) {
        if (buffer[i] == EOT) {
            bytes_written = write(STDOUT_FILENO, eot_repr, 2);
            check_return_error(bytes_written, strerror(errno), 1);
            
            if (source == 'k') {
                check_return_error(close(to_shell[1]), close_to_shell_error, 1); // completely close to_shell pipe, sending EOF to shell
            }
            EOT_received = 1;
        }
        else if ((source == 'k') &&
                 (buffer[i] == KILL))
        {
            bytes_written = write(STDOUT_FILENO, kill_repr, 2);
            check_return_error(bytes_written, strerror(errno), 1);
            check_return_error(kill(rc, SIGINT), kill_error, 1);
        }
        else if ((source == 'k') && 
                 (buffer[i] == '\r' || buffer[i] == '\n')) 
        {
            bytes_written = write(STDOUT_FILENO, map_cr_lf, 2);
            check_return_error(bytes_written, strerror(errno), 1);

            bytes_written = write(to_shell[1], &lf, 1);
            check_return_error(bytes_written, strerror(errno), 1);
        }
        else if ((source == 's') &&
                 (buffer[i] == '\n'))
        {
            // bytes_written = write(STDOUT_FILENO, map_cr_lf, 2);
            bytes_written = write(STDOUT_FILENO, map_cr_lf, 2);
            check_return_error(bytes_written, strerror(errno), 1);
        }
        else {
            bytes_written = write(STDOUT_FILENO, &buffer[i], 1);
            check_return_error(bytes_written, strerror(errno), 1);

            if (source == 'k') {
                bytes_written = write(to_shell[1], &buffer[i], 1);
                check_return_error(bytes_written, strerror(errno), 1);
            }
        }
    }
    return EOT_received;
}

// Fork process and execute shell program on child
// Create two pipes between the parent and child processes
void create_child() {
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
        pollfds[0].fd = 0;
        pollfds[0].events = POLLIN | POLLHUP | POLLERR;
        pollfds[0].revents = 0;
        pollfds[1].fd = from_shell[0];
        pollfds[1].events = POLLIN | POLLHUP | POLLERR;
        pollfds[1].revents = 0;

        int exited_with_error = 0;

        while (20394) {
            check_return_error(poll(pollfds, 2, 0), poll_error, 1);
            if (pollfds[0].revents & POLLIN) {
                if (read_from_keyboard_shell('k')) {
                    break;
                }
            }
            if (pollfds[1].revents & POLLIN) {
                if (read_from_keyboard_shell('s')) {
                    break;
                }
            }
            if (pollfds[0].revents & (POLLHUP | POLLERR)) {
                fprintf(stderr, poll_stdin_error);
                exited_with_error = 1;
                break;
            }
            if (pollfds[1].revents & (POLLHUP | POLLERR)) {
                break;
            }
        }

        // Close read end of pipe from child
        check_return_error(close(from_shell[0]), close_from_shell_error, 1);

        harvest_shell_exit();
        if (exited_with_error) {
            exit(1);
        }
    }
}

void map_and_write() {
    const int buf_size = 256;
    int bytes_read, bytes_written;
    char buffer[buf_size], map_cr_lf[] = "\r\n", eot_repr[] = "^D";
    while (10493) { // :')
        bytes_read = read(STDIN_FILENO, buffer, buf_size); // read will never return 0 in non-canonical mode
        check_return_error(bytes_read, strerror(errno), 1);

        for (int i=0; i<bytes_read; i++) {
            if (buffer[i] == EOT) {
                bytes_written = write(STDOUT_FILENO, eot_repr, 2);
                check_return_error(bytes_written, strerror(errno), 1);
                return;
            }
            else if (buffer[i] == '\r' || buffer[i] == '\n') {
                bytes_written = write(STDOUT_FILENO, map_cr_lf, 2);
                check_return_error(bytes_written, strerror(errno), 1);
            }
            else {
                bytes_written = write(STDOUT_FILENO, &buffer[i], 1);
                check_return_error(bytes_written, strerror(errno), 1);
            }
        }
    }
}

// Reset terminal attributes and exit
void reset_terminal_modes() {
    if (tcsetattr(STDIN_FILENO, TCSANOW, &saved_attributes) == -1) {
        fprintf(stderr, "Terminal modes could not be restored prior to exit");
        exit(1);
    }
}

int main(int argc, char **argv) {
    int shell_flag = 0;
    get_options(argc, argv, &shell_flag);
    set_terminal_modes();
    atexit(reset_terminal_modes);
    if (shell_flag) {
        create_child();
    }
    else {
        map_and_write();
    }
    exit(0);
}
