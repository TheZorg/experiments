#include <stdio.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/mman.h>


int main(int argc, char **argv) {
    int fd;
    uint64_t *buf;
    uint64_t sum;
    int i;
    int length;

    length = 1073741824;
    // mmap a large file
    fd = open("large_file", O_RDONLY);
    buf = mmap(NULL, length, PROT_READ, MAP_PRIVATE, fd, 0);

    sum = 0;
    for (i = 0; i < length/sizeof(uint64_t); i++) {
        sum += buf[i];
    }

    printf("sum=%lu\n", sum);

    return 0;
}
