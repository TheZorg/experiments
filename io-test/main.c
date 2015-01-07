#include <stdio.h>
#include <locale.h>
#include <stdbool.h>
#include <fcntl.h>
#include <stdint.h>
#include <x86intrin.h>
#include <pthread.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <papi.h>
#include <getopt.h>

#define PROGNAME "io-test"

#define NUM_EVENTS 1

static const char *const progname = PROGNAME;
static const int PAGE_SIZE = 4096;
static const int DEFAULT_ITERATIONS = 10000;
static const int DEFAULT_CHUNK_SIZE = 8192000;
static const int DEFAULT_THREADS = 1;

struct vars {
    char *filename;
    int iterations;
    int threads;
    int chunk_size;
    bool verbose;
};

__attribute__((noreturn))
static void usage(void) {
    fprintf(stderr, "Usage: %s [OPTIONS] file\n", progname);
    fprintf(stderr, "\nOptions:\n\n");
    fprintf(stderr, "  --iterations, -i     set number of iterations per page\n");
    fprintf(stderr, "  --chunk-size, -c     set size of chunks\n");
    fprintf(stderr, "  --threads, -t        set number of threads\n");
    fprintf(stderr, "  --verbose, -v        set verbose output\n");
    exit(EXIT_FAILURE);
}

static void parse_opts(int argc, char **argv, struct vars *vars) {
    int opt;
    size_t loadpathlen = 0;

    struct option options[] = {
        { "help",   0, 0, 'h' },
        { "verbose",   0, 0, 'v' },
        { "iterations",   1, 0, 'i' },
        { "chunk-size",   1, 0, 'c' },
        { "threads",   1, 0, 't' },
        { 0, 0, 0, 0 },
    };
    int idx;

    while ((opt = getopt_long(argc, argv, "hvi:t:", options, &idx)) != -1) {
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
        printf("using default iterations: %d\n", vars->iterations);
    }

    if (vars->threads == 0) {
        vars->threads = DEFAULT_THREADS;
        printf("using default threads: %d\n", vars->threads);
    }

    if (vars->chunk_size == 0) {
        vars->chunk_size = DEFAULT_CHUNK_SIZE;
        printf("using default chunk size: %d\n", vars->chunk_size);
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

int main(int argc, char **argv) {
    int iterations;
    int fd;
    off_t length;
    int pages;
    uint64_t cycles = 0;

    volatile uint64_t sum;

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

    pages = length / PAGE_SIZE;
    if (vars->verbose) {
        printf("pages=%d\n", pages);
    }

    // Initialize PAPI
    int events[NUM_EVENTS] = {PAPI_TOT_INS};

    PAPI_library_init(PAPI_VER_CURRENT);
    PAPI_thread_init(pthread_self);

    omp_set_num_threads(vars->threads);
#pragma omp parallel reduction(+:sum)
    {
        int i, j;
        long long int values[NUM_EVENTS];
        off_t remaining = length;
        char *buf;
        off_t to_read;
        int pages;

        PAPI_start_counters(events, NUM_EVENTS);
        while (remaining > 0) {
            to_read = remaining > vars->chunk_size ? vars->chunk_size : remaining;
            remaining -= to_read;
            buf = mmap(NULL, to_read, PROT_READ, MAP_PRIVATE , fd, 0);
            madvise(buf, to_read, MADV_SEQUENTIAL);
            pages = 0;
            sum = 0;
#pragma omp for
            for (i = 0; i < to_read; i+= 4096) {
                // Use only one byte per page
                sum += buf[i];
                for (j = 0; j < vars->iterations; j++) {
                    sum++;
                }
                pages++;
            }
            munmap(buf, to_read);
        }
        PAPI_read_counters(values, NUM_EVENTS);
        if (vars->verbose) {
            printf("Thread %d pages:%'d instr/page:%'lld total instr:%'lld\n", omp_get_thread_num(), pages, values[0]/pages, values[0]);
        }
    }
    printf("sum=%'lu\n", sum);

    return 0;
}
