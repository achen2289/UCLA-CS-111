// NAME: Alex Chen
// EMAIL: achen2289@gmail.com
// ID: 005299047

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <time.h>


int opt_yield = 0;
char *synchro = '\0';
pthread_mutex_t mutex;
long lock = 0;
const char add_csv_file[] = "lab2_add.csv";
const char correct_usage[] = "./lab2a_add.c [--threads=#] [--iterations=#]";

struct thread_args {
    long long *arg1;
    int *arg2;
};

// Check for error, and reset terminal modes before exiting
void check_return_error(int status, const char *msg, int exit_status) {
    if (status < 0) {
        fprintf(stderr, "%s\n", msg);
        exit(exit_status);
    }
}

// Parse all options and save in function variables
void get_options(int argc, char *argv[], int *threads, int *iterations) {
    int c;
    const char optstring[] = ":t:i:y:s:";
    struct option longopts[] = {
        {"threads", required_argument, NULL, 't'},
        {"iterations", required_argument, NULL, 'i'},
        {"yield", no_argument, &opt_yield, 1},
        {"sync", required_argument, NULL, 's'},
        {0, 0, 0, 0}
    };
    char mu[] = "m", sp[] = "s", c_atomic[] = "c";
    while ((c = getopt_long(argc, argv, optstring, longopts, NULL)) != -1) {
        switch (c) {
            case 't': // input option found
                *threads = atoi(optarg);
                break;
            case 'i':
                *iterations = atoi(optarg);
                break;
            case 's':
                if (!
                     (strcmp(optarg, mu) == 0 || 
                      strcmp(optarg, sp) == 0 || 
                      strcmp(optarg, c_atomic) == 0)
                    ) 
                {
                    fprintf(stderr, "%s: option '-%c' has invalid argument", argv[0], optopt);
                    exit(1);
                }
                synchro = optarg;
                if (*synchro == 'm') {
                    pthread_mutex_init(&mutex, NULL);
                }
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
}

void add(long long *pointer, long long value) {
    long long sum = *pointer + value;
    if (opt_yield) {
        sched_yield();
    }
    *pointer = sum;
}

void add_mutex(long long *pointer, long long value) {
    pthread_mutex_lock(&mutex);
    long long sum = *pointer + value;
    if (opt_yield) {
        sched_yield();
    }
    *pointer = sum;
    pthread_mutex_unlock(&mutex);
}

void add_splock(long long *pointer, long long value) {
    while (__sync_lock_test_and_set(&lock, 1));
    long long sum = *pointer + value;
    if (opt_yield) {
        sched_yield();
    }
    *pointer = sum;
    __sync_lock_release(&lock);
}

void add_atomic(long long *pointer, long long value) {
    long long prev, sum;
    do {
        prev = *pointer;
        sum = prev + value;
        if (opt_yield) {
            sched_yield();
        }
    }
    while (__sync_val_compare_and_swap(pointer, prev, sum) != prev);
}

void *add_helper(void *counter_iter) {
    struct thread_args *args = (struct thread_args *)counter_iter;
    int iterations = *(args->arg2);
    if (synchro == NULL) { // no synchronization
        for (int i=0; i<iterations; i++) {
            add(args->arg1, 1);
        }
        for (int i=0; i<iterations; i++) {
            add(args->arg1, -1);
        }
    }
    else if (*synchro == 'm') { // mutex
        for (int i=0; i<iterations; i++) {
            add_mutex(args->arg1, 1);
        }
        for (int i=0; i<iterations; i++) {
            add_mutex(args->arg1, -1);
        }
    }
    else if (*synchro == 's') { // spin lock
        for (int i=0; i<iterations; i++) {
            add_splock(args->arg1, 1);
        }
        for (int i=0; i<iterations; i++) {
            add_splock(args->arg1, -1);
        }
    }
    else /* if (*synchro == 'c') */ { // atomicity
        for (int i=0; i<iterations; i++) {
            add_atomic(args->arg1, 1);
        }
        for (int i=0; i<iterations; i++) {
            add_atomic(args->arg1, -1);
        }
    }
    return counter_iter;
}

long create_threads(int threads, int *iterations, long long *counter) {
    struct thread_args counter_iter = {counter, 
                                       iterations};
    pthread_t all_threads[threads];
    struct timespec start, end;

    check_return_error(clock_gettime(CLOCK_MONOTONIC, &start), strerror(errno), 1);
    for (int i=0; i<threads; i++) {
        int ret = pthread_create(&all_threads[i], NULL, add_helper, (void *)&counter_iter);
        check_return_error(ret, strerror(errno), -1);
    }
    for (int i=0; i<threads; i++) {
        int ret = pthread_join(all_threads[i], NULL);
        check_return_error(ret, strerror(errno), -1);
    }
    check_return_error(clock_gettime(CLOCK_MONOTONIC, &end), strerror(errno), 1);
    
    long runtime = 1000000000L * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
    return runtime;
}

void print_csv_record(int csv_fp, char *name, int threads, int iterations, long runtime, long counter) {
    long ops = threads * iterations * 2;
    long time_per_op = runtime / ops;
    char str[10000];
    sprintf(str, 
            "%s,%d,%d,%ld,%ld,%ld,%ld\n", 
            name, 
            threads, 
            iterations, 
            ops, 
            runtime, 
            time_per_op, 
            counter);
    fprintf(stdout, str);
    check_return_error(write(csv_fp, str, strlen(str)), strerror(errno), 1);
}

int main(int argc, char **argv) {
    int threads = 1, iterations = 1;
    long long counter = 0;
    get_options(argc, argv, &threads, &iterations);
    long runtime = create_threads(threads, &iterations, &counter);
    char test_name[10000];
    if (opt_yield && synchro == NULL) { // yield, no sync
        strcpy(test_name, "add-yield-none");
    }
    else if (!opt_yield && synchro == NULL) { // no yield, no sync
        strcpy(test_name, "add-none");
    }
    else if (opt_yield && *synchro == 'm') { // yield and mutex lock
        strcpy(test_name, "add-yield-m");
    }
    else if (opt_yield && *synchro == 's') { // yield and spin lock
        strcpy(test_name, "add-yield-s");
    }
    else if (opt_yield && *synchro == 'c') { // yield and atomicity
        strcpy(test_name, "add-yield-c");
    }
    else if (*synchro == 'm') { // no yield, mutex lock
        strcpy(test_name, "add-m");
    }
    else if (*synchro == 's') { // no yield, spin lock
        strcpy(test_name, "add-s");
    }
    else /* if (*synchro == 'c') */ { // no yield, atomicity
        strcpy(test_name, "add-c");
    }
    int add_csv_fp = open(add_csv_file, O_RDWR | O_CREAT | O_APPEND, S_IRWXU);
    check_return_error(add_csv_fp, strerror(errno), 1);
    print_csv_record(add_csv_fp, test_name, threads, iterations, runtime, counter);
    exit(0);
}