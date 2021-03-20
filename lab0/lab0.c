// NAME: Alex Chen
// EMAIL: achen2289@gmail.com
// ID: 005299047

#include <stdio.h> // fprintf(3)
#include <stdlib.h> // exit(2)
#include <getopt.h> // getopt_long()
#include <string.h> // strlen()
#include <assert.h> // assert()
#include <errno.h> // errno
#include <signal.h> // signal(2)

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

char correct_usage[] = "./main --input=[filename] --output=[filename] --segfault --catch";
char file_open_error[] = "The file \"%s\" could not be opened\n";
char cannot_use_as_stdin[] = "The file \"%s\" could not be used as standard input\n";
char cannot_use_as_stdout[] = "The file \"%s\" could not be used as standard output\n";

// If -1 was returned, print error_msg and exit with error_status
void check_return_error(int return_status, char *error_msg, char *file, int error_status) {
    if (return_status == -1) {
        fprintf(stderr, error_msg, file);
        exit(error_status);
    }
}

// Parse all options and save in function variables
void get_options(int argc, char *argv[], char **input, char **output, int *segfault, int *catch) {
    int c;
    const char *optstring = ":i:o:sc";
    struct option longopts[] = {
        {"input", required_argument, NULL, 'i'},
        {"output", required_argument, NULL, 'o'},
        {"segfault", no_argument, segfault, 1},
        {"catch", no_argument, catch, 1},
        {0, 0, 0, 0}
    };
    while ((c = getopt_long(argc, argv, optstring, longopts, NULL)) != -1) {
        switch (c) {
            case 'i': // input option found
                *input = optarg;
                break;
            case 'o': // output option found
                *output = optarg;
                break;
            case ':': // missing option argument
                fprintf(stderr, "%s: option '-%c' requires an argument\n", argv[0], optopt);
                exit(1);
            case '?': // invalid option
                fprintf(stderr, "%s: option '-%c' is invalid: ignored\ncorrect usage (all options are optional): %s\n", \
                        argv[0], optopt, correct_usage);
                exit(1);
            case 0: // segfault and/or catch specified
            default:
                break;
        }
    }
}

// Redirect input file if possible to standard input (fd 0)
// Redirect output file if possible to standard output (fd 1)
// input/output are NULL if not specified
void file_redirection(char *input, char *output) {
    if (input != NULL) {
        int fd_i = open(input, O_RDONLY);
        check_return_error(fd_i, file_open_error, input, 2);

        int fd_i2 = dup2(fd_i, 0);
        check_return_error(fd_i2, cannot_use_as_stdin, input, 2);

        int fd_close_i = close(fd_i);
        check_return_error(fd_close_i, cannot_use_as_stdin, input, 2);
    }
    if (output != NULL) {
        int fd_o = open(output, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU);
        check_return_error(fd_o, file_open_error, output, 3);

        int fd_o2 = dup2(fd_o, 1);
        check_return_error(fd_o2, cannot_use_as_stdout, output, 3);

        int fd_close_o = close(fd_o);
        check_return_error(fd_close_o, cannot_use_as_stdout, output, 3);
    }
}

// Exit to prevent segmentation fault
void catch_seg_fault(int signum) {
    if (signum == SIGSEGV) {
        fprintf(stderr, "Segmentation fault was caught successfully\n");
        exit(4);
    }
}

// Force a segmentation fault
// Catch segmentation fault iff --catch was specified
void try_seg_fault(int segfault, int catch) {
    if (!segfault) return;
    if (catch) {
        signal(SIGSEGV, catch_seg_fault);
    }
    char *seg_fault = '\0';
    *seg_fault = 'f'; // force seg fault by dereferencing null pointer
}

// Read from standard input (fd 0)
// Write to standard output (fd 1)
void read_and_write() {
    int bytes_read, bytes_written, buf_size = 1000;
    do {
        char buffer[buf_size];
        bytes_read = read(0, buffer, buf_size);
        if (bytes_read == -1) {
            fprintf(stderr, "%s\n", strerror(errno));
            exit(5); // exit code of 5 is a sys call error
        }
        bytes_written = write(1, buffer, bytes_read);
        if (bytes_written == -1) {
            fprintf(stderr, "%s\n", strerror(errno));
            exit(5);
        }
    }
    while (bytes_read > 0);
}

int main(int argc, char *argv[]) {
    char *input = '\0', *output = '\0';
    int segfault = 0, catch = 0;
    get_options(argc, argv, &input, &output, &segfault, &catch);
    file_redirection(input, output);
    try_seg_fault(segfault, catch);
    read_and_write();
    exit(0);
}