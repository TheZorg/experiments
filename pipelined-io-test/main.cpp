#include <iostream>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>

#include <tbb/tbb.h>
#include <getopt.h>

#define PROGNAME "pipelined-io-test"

static const char *const progname = PROGNAME;

static const int PAGE_SIZE = 4096;

static const int BYTES_IN_MBYTE = 1000000;
static const int NSECS_IN_MSEC = 1000000;
static const int NSECS_IN_SEC = 1000000000;

static const int DEFAULT_ITERATIONS = 10000;
static const int DEFAULT_THREADS = 1;
static const int DEFAULT_NTOKENS = 32;
static const int DEFAULT_META_CHUNK_SIZE = 2048 * PAGE_SIZE;
static const int DEFAULT_CHUNK_SIZE = 32 * PAGE_SIZE;

struct Vars {
    std::string filename;
    int iterations = 0;
    int threads = 0;
    int ntokens = 0;
    off_t meta_chunk_size = 0;
    off_t chunk_size = 0;
    bool verbose = false;
    bool prefault = false;
};

__attribute__((noreturn))
static void usage(void) {
    fprintf(stderr, "Usage: %s [OPTIONS] file\n", progname);
    fprintf(stderr, "\nOptions:\n\n");
    fprintf(stderr, "  --iterations, -i         set number of iterations per page\n");
    fprintf(stderr, "  --meta-chunk-size, -m    set size of metachunks\n");
    fprintf(stderr, "  --chunk-size, -c         set size of chunks\n");
    fprintf(stderr, "  --threads, -t            set number of threads\n");
    fprintf(stderr, "  --ntokens, -n            set number of tokens in pipeline\n");
    fprintf(stderr, "  --prefault, -p           prefault pages when reading file\n");
    fprintf(stderr, "  --verbose, -v            set verbose output\n");
    exit(EXIT_FAILURE);
}

static void parse_opts(int argc, char **argv, Vars &vars) {
    int opt;
    size_t loadpathlen = 0;

    struct option options[] = {
        { "help",   0, 0, 'h' },
        { "verbose",   0, 0, 'v' },
        { "prefault",   0, 0, 'p' },
        { "iterations",   1, 0, 'i' },
        { "meta-chunk-size",   1, 0, 'm' },
        { "chunk-size",   1, 0, 'c' },
        { "threads",   1, 0, 't' },
        { "ntokens",   1, 0, 'n' },
        { 0, 0, 0, 0 },
    };
    int idx;

    while ((opt = getopt_long(argc, argv, "hvpi:n:t:m:c:", options, &idx)) != -1) {
        switch (opt) {
            case 'i':
                vars.iterations = atoi(optarg);
                break;
            case 't':
                vars.threads = atoi(optarg);
                break;
            case 'n':
                vars.ntokens = atoi(optarg);
                break;
            case 'c':
                vars.chunk_size = atoi(optarg);
                break;
            case 'm':
                vars.meta_chunk_size = atoi(optarg);
                break;
            case 'v':
                vars.verbose = true;
                break;
            case 'p':
                vars.prefault = true;
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
        vars.filename = argv[optind];
    }

    // Default values
    if (vars.iterations == 0) {
        vars.iterations = DEFAULT_ITERATIONS;
        if (vars.verbose) {
            printf("using default iterations: %d\n", vars.iterations);
        }
    }

    if (vars.threads == 0) {
        vars.threads = DEFAULT_THREADS;
        if (vars.verbose) {
            printf("using default threads: %d\n", vars.threads);
        }
    }

    if (vars.ntokens == 0) {
        vars.ntokens = DEFAULT_NTOKENS;
        if (vars.verbose) {
            printf("using default ntokens: %d\n", vars.ntokens);
        }
    }

    if (vars.chunk_size == 0) {
        vars.chunk_size = DEFAULT_CHUNK_SIZE;
        if (vars.verbose) {
            printf("using default chunk size: %ld\n", vars.chunk_size);
        }
    }

    if (vars.meta_chunk_size == 0) {
        vars.meta_chunk_size = DEFAULT_META_CHUNK_SIZE;
        if (vars.verbose) {
            printf("using default meta chunk size: %ld\n", vars.meta_chunk_size);
        }
    }
}

struct MetaChunk {
    uint8_t *start = NULL;
    off_t size = 0;
    off_t offset = 0;
    off_t processed = 0;
};

MetaChunk metachunk;

// One chunk will be at most 32 pages
struct Chunk {
    uint8_t *start = NULL;
    off_t size = 0;
};

class InputFunctor {
    public:
        InputFunctor(int fd, off_t filesize, const Vars &vars);
        InputFunctor(const InputFunctor &other);
        ~InputFunctor();
        Chunk operator()(tbb::flow_control &fc) const;
    private:
        int fd;
        off_t filesize;
        const Vars &vars;
};

InputFunctor::InputFunctor(int fd, off_t filesize, const Vars &vars) 
    : fd(fd), filesize(filesize), vars(vars) {
}

InputFunctor::InputFunctor(const InputFunctor &other) : fd(other.fd), filesize(other.filesize), vars(other.vars) {
}

InputFunctor::~InputFunctor() {
}

Chunk InputFunctor::operator()(tbb::flow_control &fc) const {
    // Ending condition : we have read the last chunk of 
    // the last metachunk
    if (metachunk.offset >= filesize && metachunk.processed >= metachunk.size) {
        fc.stop();
        return Chunk();
    }

    // Check if we need to mmap a new metachunk
    // Happens if no metachunk is mmap'd and
    // when we have processed all the chunks
    if (metachunk.start == NULL || metachunk.processed >= metachunk.size) {
        // Unmap old one, if necessary
        if (metachunk.start != NULL) {
            // TODO: No unmapping at the moment
            //munmap(metachunk.start, metachunk.size);
        }
        off_t remaining = filesize - metachunk.offset;
        metachunk.size = remaining > vars.meta_chunk_size ? vars.meta_chunk_size : remaining;
        int flags = MAP_PRIVATE;
        if (vars.prefault) flags |= MAP_POPULATE;
        metachunk.start = static_cast<uint8_t*>(mmap(NULL, metachunk.size, PROT_READ, flags, fd, metachunk.offset));
        if (metachunk.start == MAP_FAILED) {
            perror("mmap");
            exit(EXIT_FAILURE);
        }
        metachunk.offset += metachunk.size;
        metachunk.processed = 0;
        madvise(metachunk.start, metachunk.size, MADV_SEQUENTIAL);
    }

    // Dispatch the next chunk
    Chunk c;
    off_t remaining = metachunk.size - metachunk.processed;
    c.start = metachunk.start + metachunk.processed;
    c.size = remaining > vars.chunk_size ? vars.chunk_size : remaining;
    metachunk.processed += c.size;

    return c;
}

class ProcessFunctor {
public:
    ProcessFunctor(const Vars &vars);
    ProcessFunctor(const ProcessFunctor &other);
    uint64_t operator()(Chunk input) const;

private:
    const Vars &vars;
};

ProcessFunctor::ProcessFunctor(const Vars &vars) : vars(vars) {
}

ProcessFunctor::ProcessFunctor(const ProcessFunctor &other) : vars(other.vars) {
}

uint64_t ProcessFunctor::operator()(Chunk input) const {
    uint64_t sum = 0;
    for (int i = 0; i < input.size; i += PAGE_SIZE) {
        sum += input.start[i];
        for (int j = 0; j < vars.iterations; j++) {
            sum++;
            asm("");
        }
    }
    return sum;
}

uint64_t global_sum = 0;

class OutputFunctor {
public:
    void operator()(uint64_t input) const;
};

void OutputFunctor::operator()(uint64_t input) const {
    global_sum += input;
}

off_t get_filesize(int fd) {
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
    Vars vars;
    parse_opts(argc, argv, vars);

    int fd = open(vars.filename.c_str(), O_RDONLY);
    if (fd == -1) {
        std::cerr << "Error: cannot open file " << vars.filename << std::endl;
        exit(EXIT_FAILURE);
    }

    off_t filesize = get_filesize(fd);
    if (filesize == -1) {
        std::cerr << "Error: cannot get file size." << std::endl;
        exit(EXIT_FAILURE);
    }

    if (vars.chunk_size % PAGE_SIZE != 0) {
        vars.chunk_size += (PAGE_SIZE - (vars.chunk_size % PAGE_SIZE));
        printf("Growing chunk size to nearest page multiple: %'jd\n", vars.chunk_size);
    }

    if (vars.meta_chunk_size % PAGE_SIZE != 0) {
        vars.meta_chunk_size += (PAGE_SIZE - (vars.meta_chunk_size % PAGE_SIZE));
        printf("Growing meta chunk size to nearest page multiple: %'jd\n", vars.chunk_size);
    }

    tbb::task_scheduler_init init(vars.threads);

    tbb::filter_t<void, Chunk> in(tbb::filter::serial_in_order, InputFunctor(fd, filesize, vars));
    tbb::filter_t<Chunk, uint64_t> process(tbb::filter::parallel, ProcessFunctor(vars));
    tbb::filter_t<uint64_t, void> out(tbb::filter::serial_out_of_order, OutputFunctor());
    tbb::filter_t<void,void> merge = in & process & out;

    timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    tbb::parallel_pipeline(vars.ntokens, merge);

    clock_gettime(CLOCK_MONOTONIC, &end);

    std::cout << "sum=" << global_sum << std::endl;

    timespec diff = time_diff(start, end);

    double time = (double)diff.tv_sec + ((double)diff.tv_nsec / (double)NSECS_IN_SEC);
    printf("Time (s): %ld.%ld\n", diff.tv_sec, diff.tv_nsec / NSECS_IN_MSEC);
    printf("Bandwidth (MB/s): %f\n", ((double)filesize/time)/(double)BYTES_IN_MBYTE);
}
