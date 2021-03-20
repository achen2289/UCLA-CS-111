// NAME: Alex Chen
// EMAIL: achen2289@gmail.com
// ID: 005299047

#include "SortedList.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <getopt.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

int threads, iterations, lists;
int yield_i, yield_d, yield_l, opt_yield;
char *synchro = '\0';
pthread_mutex_t *mutex;
long *lock;
SortedList_t *head;
SortedListElement_t *pool;
unsigned long *wait_times;

const char list_csv_file[] = "lab2b_list.csv";
const char correct_usage[] = "./lab2_list [--threads=#] [--iterations=#] [--yield=[idl]]\n";
const char malloc_error[] = "ERROR: Additional memory could not be allocated\n";
const char delete_element_error[] = "ERROR: Corrupted pointers upon deleting element from list\n";
const char lookup_element_error[] = "ERROR: Element could not be looked up in list\n";
const char list_length_error[] = "ERROR: List's length is incorrect\n";
const char segfault_error[] = "ERROR: Segmentation fault\n";

// Check for error, and reset terminal modes before exiting
void check_return_error(int status, const char *msg, int exit_status) {
    if (status < 0) {
        fprintf(stderr, "%s\n", msg);
        exit(exit_status);
    }
}

void segfault_handler() {
    fprintf(stderr, segfault_error);
    exit(2);
}

// Parse all options and save in function variables
void get_options(int argc, char *argv[]) {
    int c;
    const char optstring[] = ":t:i:y:s:l:";
    struct option longopts[] = {
        {"threads", required_argument, NULL, 't'},
        {"iterations", required_argument, NULL, 'i'},
        {"yield", required_argument, NULL, 'y'},
        {"sync", required_argument, NULL, 's'},
        {"lists", required_argument, NULL, 'l'},
        {0, 0, 0, 0}
    };
    char mu[] = "m", sp[] = "s";
    while ((c = getopt_long(argc, argv, optstring, longopts, NULL)) != -1) {
        switch (c) {
            case 't': // input option found
                threads = atoi(optarg);
                break;
            case 'i':
                iterations = atoi(optarg);
                break;
            case 'y':
                for (size_t i=0; i<strlen(optarg); i++) {
                    if (optarg[i] == 'i') {
                        opt_yield |= INSERT_YIELD;
                        yield_i = 1;
                    }
                    else if (optarg[i] == 'd') {
                        opt_yield |= DELETE_YIELD;
                        yield_d = 1;
                    }
                    else if (optarg[i] == 'l') {
                        opt_yield |= LOOKUP_YIELD;
                        yield_l = 1;
                    }
                    else {
                        fprintf(stderr, "%s: option '-%c' is invalid: ignored\ncorrect usage: %s\n", \
                                argv[0], optopt, correct_usage);
                        exit(1);
                    }
                }
                break;
            case 's':
                if (!(strcmp(optarg, mu) == 0 || strcmp(optarg, sp) == 0)) {
                    fprintf(stderr, "%s: option '-%c' has invalid argument\n", argv[0], optopt);
                    exit(1);
                }
                synchro = optarg;
                break;
            case 'l':
                lists = atoi(optarg);
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

// Hash function credits: http://www.cse.yorku.ca/~oz/hash.html
// "djb2" by Dan Bernstein
unsigned long hasher(const char *key) {
    unsigned long hash = 5381;
    int c;
    while ((c = *key++)) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }
    return hash % lists;
}

// Initialize mutex, locks, thread wait times, list heads, list element keys
// Randomize length and chars of keys
void initialize_list() {
    int size = threads * iterations;
    if (synchro && *synchro == 'm') {
        mutex = (pthread_mutex_t *)malloc(lists * sizeof(pthread_mutex_t));
        if (!mutex) {
            check_return_error(-1, malloc_error, 1);
        }
        for (int i=0; i<lists; i++) {
            check_return_error(pthread_mutex_init(&mutex[i], NULL), strerror(errno), 1);
        }
    }
    else if (synchro && *synchro == 's') {
        lock = (long *)malloc(lists * sizeof(long));
        if (!lock) {
            check_return_error(-1, malloc_error, 1);
        }
        for (int i=0; i<lists; i++) {
            lock[i] = 0;
        }
    }
    wait_times = (unsigned long *)malloc(threads * sizeof(unsigned long));
    if (!wait_times) {
        check_return_error(-1, malloc_error, 1);
    }
    for (int i=0; i<threads; i++) {
        wait_times[i] = 0;
    }
    head = (SortedList_t *)malloc(lists * sizeof(SortedList_t)); // allocate memory for head
    if (!head) {
        check_return_error(-1, malloc_error, 1);
    }
    for (int i=0; i<lists; i++) {
        head[i].prev = &head[i];
        head[i].next = &head[i];
        head[i].key = NULL;
    }
    pool = (SortedListElement_t *)malloc(size * sizeof(SortedListElement_t)); // allocate memory for elements/threads
    if (!pool) {
        check_return_error(-1, malloc_error, 1);
    }
    srand(time(NULL)); // generate random seed
    for (int i=0; i<size; i++) {
        SortedListElement_t *curr = &pool[i]; // current list element

        int max_word_size = 20, word_size = rand() % max_word_size + 5;
        char *random_word = (char *)malloc(word_size * sizeof(char));
        if (!random_word) {
            check_return_error(-1, malloc_error, 1);
        }
        for (int j=0; j<word_size-1; j++) { // generate random word
            random_word[j] = 'a' + rand() % 25;
        }
        random_word[word_size-1] = '\0';
        curr->key = random_word;
    }
}

// Thread worker function
void *list_op(void *start_thread) {
    int thread_number = *((int *)start_thread);
    int start_key = thread_number * iterations;
    struct timespec start, end;
    long runtime;

    for (int i=start_key; i<start_key+iterations; i++) {
        SortedListElement_t *curr = &pool[i];
        unsigned long hash = hasher(curr->key);
        if (synchro && *synchro == 'm') {
            check_return_error(clock_gettime(CLOCK_MONOTONIC, &start), strerror(errno), 1);
            check_return_error(pthread_mutex_lock(&mutex[hash]), strerror(errno), 1);
            check_return_error(clock_gettime(CLOCK_MONOTONIC, &end), strerror(errno), 1);
        }
        else if (synchro && *synchro == 's') {
            check_return_error(clock_gettime(CLOCK_MONOTONIC, &start), strerror(errno), 1);
            while (__sync_lock_test_and_set(&lock[hash], 1));
            check_return_error(clock_gettime(CLOCK_MONOTONIC, &end), strerror(errno), 1);
        }
        runtime = 1000000000L * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
        wait_times[thread_number] += runtime;

        SortedList_insert(&head[hash], &pool[i]);

        if (synchro && *synchro == 'm') {
            check_return_error(pthread_mutex_unlock(&mutex[hash]), strerror(errno), 1);
        }
        else if (synchro && *synchro == 's') {
            __sync_lock_release(&lock[hash]);
        }
    }

    int total_length = 0;

    for (int i=0; i<lists; i++) {
        if (synchro && *synchro == 'm') {
            check_return_error(clock_gettime(CLOCK_MONOTONIC, &start), strerror(errno), 1);
            check_return_error(pthread_mutex_lock(&mutex[i]), strerror(errno), 1);
            check_return_error(clock_gettime(CLOCK_MONOTONIC, &end), strerror(errno), 1);
        }
        else if (synchro && *synchro == 's') {
            check_return_error(clock_gettime(CLOCK_MONOTONIC, &start), strerror(errno), 1);
            while (__sync_lock_test_and_set(&lock[i], 1));
            check_return_error(clock_gettime(CLOCK_MONOTONIC, &end), strerror(errno), 1);
        }
        runtime = 1000000000L * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
        wait_times[thread_number] += runtime;

        int length = SortedList_length(&head[i]);
        check_return_error(length, list_length_error, 2);
        total_length += length;

        if (synchro && *synchro == 'm') {
            check_return_error(pthread_mutex_unlock(&mutex[i]), strerror(errno), 1);
        }
        else if (synchro && *synchro == 's') {
            __sync_lock_release(&lock[i]);
        }
    }
    
    for (int i=start_key; i<start_key+iterations; i++) {
        SortedListElement_t *curr = &pool[i];
        unsigned long hash = hasher(curr->key);

        if (synchro && *synchro == 'm') {
            check_return_error(clock_gettime(CLOCK_MONOTONIC, &start), strerror(errno), 1);
            check_return_error(pthread_mutex_lock(&mutex[hash]), strerror(errno), 1);
            check_return_error(clock_gettime(CLOCK_MONOTONIC, &end), strerror(errno), 1);
        }
        else if (synchro && *synchro == 's') {
            check_return_error(clock_gettime(CLOCK_MONOTONIC, &start), strerror(errno), 1);
            while (__sync_lock_test_and_set(&lock[hash], 1));
            check_return_error(clock_gettime(CLOCK_MONOTONIC, &end), strerror(errno), 1);
        }
        runtime = 1000000000L * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
        wait_times[thread_number] += runtime;

        SortedListElement_t *res = SortedList_lookup(&head[hash], pool[i].key);
        if (res == NULL) {
            check_return_error(-1, lookup_element_error, 2);
        }
        int ret = SortedList_delete(res);
        if (ret == 1) {
            check_return_error(-1, delete_element_error, 2);
        }

        if (synchro && *synchro == 'm') {
            check_return_error(pthread_mutex_unlock(&mutex[hash]), strerror(errno), 1);
        }
        if (synchro && *synchro == 's') {
            __sync_lock_release(&lock[hash]);
        }
    }

    return NULL;
}

// Initialize and run threads
// Keep track of total time spent waiting for locks
long create_run_threads() {
    pthread_t all_threads[threads];
    struct timespec start, end;

    check_return_error(clock_gettime(CLOCK_MONOTONIC, &start), strerror(errno), 1);
    int thread_indices[threads];
    for (int i=0; i<threads; i++) {
        thread_indices[i] = i;
        int ret = pthread_create(&all_threads[i], NULL, list_op, (void *)&thread_indices[i]);
        check_return_error(ret, strerror(errno), -1);
    }
    for (int i=0; i<threads; i++) {
        int ret = pthread_join(all_threads[i], NULL);
        check_return_error(ret, strerror(errno), -1);
    }
    check_return_error(clock_gettime(CLOCK_MONOTONIC, &end), strerror(errno), 1);

    for (int i=0; i<lists; i++) {
        if (SortedList_length(&head[i]) != 0) {
            check_return_error(-1, list_length_error, 2);
        }
    }
    
    long runtime = 1000000000L * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
    return runtime;
}

void print_csv_record(int csv_fp, char *name, int threads, int iterations, int num_lists, long runtime, unsigned long total_wait_time) {
    long ops = threads * iterations * 3;
    long time_per_op = runtime / ops;
    // A lock for each insert and each pair of lookup/deletes
    // A lock for each length operation (one per thread)
    long num_lock_ops = threads * iterations * 2 + threads; 
    long wait_per_lock = total_wait_time / num_lock_ops;
    char str[10000];
    sprintf(str, 
            "%s,%d,%d,%d,%ld,%ld,%ld,%ld\n",
            name,
            threads,
            iterations,
            num_lists,
            ops,
            runtime,
            time_per_op,
            wait_per_lock
            );
    fprintf(stdout, str);
    check_return_error(write(csv_fp, str, strlen(str)), strerror(errno), 1);
}

void free_list() {
    free(head);
    free(pool);
}

int main(int argc, char **argv) {
    signal(SIGSEGV, segfault_handler);
    threads = 1, iterations = 1, lists = 1;
    yield_i = 0, yield_d = 0, yield_l = 0;
    opt_yield = 0;
    
    get_options(argc, argv);
    initialize_list();
    long runtime = create_run_threads();

    int list_csv_fp = open(list_csv_file, O_RDWR | O_CREAT | O_APPEND, S_IRWXU);
    check_return_error(list_csv_fp, strerror(errno), 1);

    unsigned long total_wait_time = 0;
    for (int i=0; i<threads; i++) {
        total_wait_time += wait_times[i];
    }

    char test_name[10000];
    strcpy(test_name, "list-");
    if (yield_i) {
        strcat(test_name, "i");
    }
    if (yield_d) {
        strcat(test_name, "d");
    }
    if (yield_l) {
        strcat(test_name, "l");
    }
    if (yield_i + yield_d + yield_l == 0) {
        strcat(test_name, "none");
    }
    strcat(test_name, "-");
    if (!synchro) {
        strcat(test_name, "none");
    }
    else {
        strcat(test_name, synchro);
    }
    print_csv_record(list_csv_fp, test_name, threads, iterations, lists, runtime, total_wait_time);
    free_list();
    exit(0);
}