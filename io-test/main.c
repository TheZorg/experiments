#include <stdio.h>
#include <locale.h>
#include <stdbool.h>
#include <fcntl.h>
#include <stdint.h>
#include <x86intrin.h>
#include <pthread.h>
#include <time.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <papi.h>
#include <getopt.h>

#define TRACEPOINT_DEFINE
#define TRACEPOINT_CREATE_PROBES
#include "tp.h"

#define PROGNAME "io-test"

#define NUM_EVENTS 1
#define NSECS_IN_MSEC 1000000
#define NSECS_IN_SEC 1000000000
#define BYTES_IN_MBYTE 1000000

static const char *const progname = PROGNAME;
static const int MY_PAGE_SIZE = 4096;
static const int DEFAULT_ITERATIONS = 10000;
static const int DEFAULT_CHUNK_SIZE = 2048 * MY_PAGE_SIZE;
static const int DEFAULT_THREADS = 1;

struct vars {
    char *filename;
    int iterations;
    int threads;
    off_t chunk_size;
    bool verbose;
    bool worst_case;
    bool prefault;
};

__attribute__((noreturn))
static void usage(void) {
    fprintf(stderr, "Usage: %s [OPTIONS] file\n", progname);
    fprintf(stderr, "\nOptions:\n\n");
    fprintf(stderr, "  --iterations, -i     set number of iterations per page\n");
    fprintf(stderr, "  --chunk-size, -c     set size of chunks\n");
    fprintf(stderr, "  --threads, -t        set number of threads\n");
    fprintf(stderr, "  --worst-case, -w     force worst case performance\n");
    fprintf(stderr, "  --prefault, -p       prefault pages when reading file\n");
    fprintf(stderr, "  --verbose, -v        set verbose output\n");
    exit(EXIT_FAILURE);
}

static void parse_opts(int argc, char **argv, struct vars *vars) {
    int opt;
    size_t loadpathlen = 0;

    struct option options[] = {
        { "help",   0, 0, 'h' },
        { "verbose",   0, 0, 'v' },
        { "worst-case",   0, 0, 'w' },
        { "prefault",   0, 0, 'p' },
        { "iterations",   1, 0, 'i' },
        { "chunk-size",   1, 0, 'c' },
        { "threads",   1, 0, 't' },
        { 0, 0, 0, 0 },
    };
    int idx;

    while ((opt = getopt_long(argc, argv, "hvwpi:t:c:", options, &idx)) != -1) {
        switch (opt) {
            case 'i':
                vars->iterations = atoi(optarg);
                break;
            case 't':
                vars->threads = atoi(optarg);
                break;
            case 'c':
                vars->chunk_size = atoi(optarg);
                break;
            case 'v':
                vars->verbose = true;
                break;
            case 'w':
                vars->worst_case = true;
                break;
            case 'p':
                vars->prefault = true;
                break;
            case 'h':
                usage();
                break;
            default:
                usage();
                break;
        }
    }

    // Non-option arg for filename
    if (optind >= argc) {
        fprintf(stderr, "File name missing.\n");
        usage();
    } else {
        vars->filename = argv[optind];
    }

    // Default values
    if (vars->iterations == 0) {
        vars->iterations = DEFAULT_ITERATIONS;
        if (vars->verbose) {
            printf("using default iterations: %d\n", vars->iterations);
        }
    }

    if (vars->threads == 0) {
        vars->threads = DEFAULT_THREADS;
        if (vars->verbose) {
            printf("using default threads: %d\n", vars->threads);
        }
    }

    if (vars->chunk_size == 0 && !vars->worst_case) {
        vars->chunk_size = DEFAULT_CHUNK_SIZE;
        if (vars->verbose) {
            printf("using default chunk size: %ld\n", vars->chunk_size);
        }
    }
}

off_t filesize(int fd) {
    struct stat stats;
    int ret = -1;
    if (fstat(fd, &stats) == 0) {
        ret = stats.st_size;
    }

    return ret;
}

struct timespec time_diff(struct timespec start,struct timespec end) {
    struct timespec ret;
    if ((end.tv_nsec - start.tv_nsec) < 0) {
        ret.tv_sec = end.tv_sec - start.tv_sec - 1;
        ret.tv_nsec = NSECS_IN_SEC + end.tv_nsec - start.tv_nsec;
    } else {
        ret.tv_sec = end.tv_sec - start.tv_sec;
        ret.tv_nsec = end.tv_nsec - start.tv_nsec;
    }
    return ret;
}

int main(int argc, char **argv) {
    tracepoint(tracekit, begin);
    int iterations;
    int fd;
    off_t length;
    int pages;
    struct timespec start, end;
    int advice, mmap_flags;

    volatile uint64_t sum = 0;

    struct vars *vars = calloc(1, sizeof(struct vars));
    parse_opts(argc, argv, vars);

    setlocale(LC_NUMERIC, "");

    fd = open(vars->filename, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "Error: cannot open file %s\n", vars->filename);
        exit(EXIT_FAILURE);
    }

    length = filesize(fd);
    if (length == -1) {
        fprintf(stderr, "Error: cannot get file size.\n");
        exit(EXIT_FAILURE);
    }

    if (vars->chunk_size == -1 || vars->worst_case) {
        vars->chunk_size = length;
    }

    if (vars->chunk_size % MY_PAGE_SIZE != 0) {
        vars->chunk_size += (MY_PAGE_SIZE - (vars->chunk_size % MY_PAGE_SIZE));
        printf("Growing chunk size to nearest page multiple: %'jd\n", vars->chunk_size);
    }

    if (vars->worst_case) {
        advice = MADV_RANDOM;
    } else {
        advice = MADV_SEQUENTIAL;
    }

    mmap_flags = MAP_PRIVATE;
    if (vars->prefault) {
        mmap_flags |= MAP_POPULATE;
    }

    pages = length / MY_PAGE_SIZE;
    if (vars->verbose) {
        printf("pages=%d\n", pages);
    }

    // Initialize PAPI
    int events[NUM_EVENTS] = {PAPI_TOT_INS};

    PAPI_library_init(PAPI_VER_CURRENT);
    PAPI_thread_init(pthread_self);

    omp_set_num_threads(vars->threads);

    clock_gettime(CLOCK_MONOTONIC, &start);

#ifndef NO_OMP
#pragma omp parallel reduction(+:sum)
#endif
    {
        int i, j;
        long long int values[NUM_EVENTS];
        uint8_t *buf;
        off_t to_read;
        int pages = 0;
        int iterations = vars->iterations;
        off_t offset = 0;

        PAPI_start_counters(events, NUM_EVENTS);
        while (offset < length) {
            off_t remaining = length - offset;
            to_read = remaining > vars->chunk_size ? vars->chunk_size : remaining;
            buf = mmap(NULL, to_read, PROT_READ, mmap_flags, fd, offset);
            if (buf == MAP_FAILED) {
                perror("mmap");
                exit(EXIT_FAILURE);
            }
            offset += to_read;
            madvise(buf, to_read, advice);

#ifndef NO_OMP
#pragma omp for
#endif
            for (i = 0; i < to_read; i+= MY_PAGE_SIZE) {
                // Use only one byte per page
                sum += buf[i];
                for (j = 0; j < iterations; j++) {
                    sum++;
                }
                pages++;
            }
            munmap(buf, to_read);
        }
        PAPI_read_counters(values, NUM_EVENTS);
        if (vars->verbose) {
            printf("Thread %d pages:%'d instr/page:%'lld total instr:%'lld\n", omp_get_thread_num(), pages, values[0]/pages, values[0]);
            printf("Thread %d sum:%'lu\n", omp_get_thread_num(), sum);
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &end);

    printf("sum=%'lu\n", sum);
    struct timespec diff = time_diff(start, end);
    double time = (double)diff.tv_sec + ((double)diff.tv_nsec / (double)NSECS_IN_SEC);
    printf("Time (s): %ld.%ld\n", diff.tv_sec, diff.tv_nsec / NSECS_IN_MSEC);
    printf("Bandwidth (MB/s): %f\n", ((double)length/time)/(double)BYTES_IN_MBYTE);

    tracepoint(tracekit, end);
    return 0;
}
