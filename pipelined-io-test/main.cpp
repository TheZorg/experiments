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

static const int PAGE_SIZE = 4096;
static const int META_CHUNK_SIZE = 2048 * PAGE_SIZE;
static const int CHUNK_SIZE = 32 * PAGE_SIZE;
static const int ITERATIONS = 1;
static const int BYTES_IN_MBYTE = 1000000;
static const int NSECS_IN_MSEC = 1000000;
static const int NSECS_IN_SEC = 1000000000;

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
        InputFunctor(int fd, off_t filesize);
        InputFunctor(const InputFunctor &other);
        ~InputFunctor();
        Chunk operator()(tbb::flow_control &fc) const;
    private:
        int fd;
        off_t filesize;
};

InputFunctor::InputFunctor(int fd, off_t filesize) 
    : fd(fd), filesize(filesize) {
}

InputFunctor::InputFunctor(const InputFunctor &other) : fd(other.fd), filesize(other.filesize) {
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
            // No unmapping at the moment
            //munmap(metachunk.start, metachunk.size);
        }
        off_t remaining = filesize - metachunk.offset;
        metachunk.size = remaining > META_CHUNK_SIZE ? META_CHUNK_SIZE : remaining;
        metachunk.start = static_cast<uint8_t*>(mmap(NULL, metachunk.size, PROT_READ, MAP_PRIVATE | MAP_POPULATE, fd, metachunk.offset));
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
    c.size = remaining > CHUNK_SIZE ? CHUNK_SIZE : remaining;
    metachunk.processed += c.size;

    return c;
}

class ProcessFunctor {
public:
    uint64_t operator()(Chunk input) const;
};

uint64_t ProcessFunctor::operator()(Chunk input) const {
    uint64_t sum = 0;
    for (int i = 0; i < input.size; i += PAGE_SIZE) {
        sum += input.start[i];
        for (int j = 0; j < ITERATIONS; j++) {
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
    int fd = open("large_file", O_RDONLY);
    off_t filesize = get_filesize(fd);
    timespec start, end;

    tbb::filter_t<void, Chunk> in(tbb::filter::serial_in_order, InputFunctor(fd, filesize));
    tbb::filter_t<Chunk, uint64_t> process(tbb::filter::parallel, ProcessFunctor());
    tbb::filter_t<uint64_t, void> out(tbb::filter::serial_out_of_order, OutputFunctor());
    tbb::filter_t<void,void> merge = in & process & out;
    clock_gettime(CLOCK_MONOTONIC, &start);
    tbb::parallel_pipeline(32, merge);
    clock_gettime(CLOCK_MONOTONIC, &end);
    std::cout << "sum=" << global_sum << std::endl;
    timespec diff = time_diff(start, end);
    double time = (double)diff.tv_sec + ((double)diff.tv_nsec / (double)NSECS_IN_SEC);
    printf("Time (s): %ld.%ld\n", diff.tv_sec, diff.tv_nsec / NSECS_IN_MSEC);
    printf("Bandwidth (MB/s): %f\n", ((double)filesize/time)/(double)BYTES_IN_MBYTE);
}
