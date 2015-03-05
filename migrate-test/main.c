#define _GNU_SOURCE
#include <pthread.h>
#include <sched.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

void *worker(void *arg) {
    FILE *fp = fopen("/dev/zero", "r");
    size_t size = 1024*1024*sizeof(char);
    char *buffer = malloc(size);

    while (true) {
        fread(buffer, size, 1, fp);
    }

    free(buffer);
    return NULL;
}

int main(int argc, char **argv) {
    cpu_set_t cpuset;
    pthread_t thread;

    pthread_create(&thread, NULL, worker, NULL);

    const int numCpus = 8;
    int i = 0;
    struct timespec timeout = { .tv_sec = 0, .tv_nsec = 1000000 };
    while (true) {
        CPU_ZERO(&cpuset);
        CPU_SET(i, &cpuset);
        i = (i + 1) % numCpus;
        pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
        nanosleep(&timeout, NULL);
    }
}
