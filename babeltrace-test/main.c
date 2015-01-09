#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <locale.h>
#include <papi.h>
#include <pthread.h>
#include <time.h>

#include <babeltrace/babeltrace.h>
#include <babeltrace/iterator.h>
#include <babeltrace/ctf/iterator.h>
#include <babeltrace/ctf/types.h>

#define NSECS_IN_MSEC 1000000
#define NSECS_IN_SEC 1000000000
#define BYTES_IN_MBYTE 1000000

uint64_t count = 0;
uint64_t instr = 0;
uint64_t size = 0;

struct node {
    int val;
    struct node *next;
};

struct list {
    struct node *head;
    struct node *tail;
};

bool contains(struct list *l, int val) {
    struct node *n = l->head;
    while (n != NULL) {
        if (n->val == val) return true;
        n = n->next;
    }
    return false;
}

void append(struct list *l, int val) {
    struct node *n = calloc(1, sizeof(struct node));
    n->val = val;
    if (l->tail == NULL) {
        l->head = n;
        l->tail = n;
    } else {
        l->tail->next = n;
        l->tail = n;
    }
}

struct list fds;

off_t filesize(int fd) {
    struct stat stats;
    int ret = -1;
    if (fstat(fd, &stats) == 0) {
        ret = stats.st_size;
    }

    return ret;
}

void seek(struct bt_stream_pos *pos, size_t index, int whence) {
    static uint64_t last_count = 0;
    struct ctf_stream_pos *p = ctf_pos(pos);
    long long int values[1] = {0};
    PAPI_read_counters(values, 1);
    instr += values[0];
    /*printf("packet size:%'lu events:%'lu instr:%'lld\n", p->content_size, count - last_count, values[0]);*/
    last_count = count;
    
    // Have we seen this stream?
    if (!contains(&fds, p->fd)) {
        size += filesize(p->fd);
        append(&fds, p->fd);
    }

    ctf_packet_seek(pos, index, whence);
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
    struct bt_context *ctx;
    struct bt_ctf_iter *iter;
    struct bt_ctf_event *ctf_event;
    char *trace_path;
    int trace_id;
    struct timespec start, end;

    if (argc != 2) {
        fprintf(stderr, "No trace path provided.\n");
        exit(EXIT_FAILURE);
    }

    trace_path = argv[1];
    setlocale(LC_NUMERIC, "");
    int events[] = {PAPI_TOT_INS};
    long long int values[1];

    PAPI_library_init(PAPI_VER_CURRENT);
    /*PAPI_thread_init(pthread_self);*/

    PAPI_start_counters(events, 1);

    clock_gettime(CLOCK_MONOTONIC, &start);
    ctx = bt_context_create();
    trace_id = bt_context_add_trace(ctx, trace_path, "ctf", seek, NULL, NULL);
    if (trace_id < 0) {
        fprintf(stderr, "Failed: bt_context_add_trace\n");
        exit(EXIT_FAILURE);
    }

    iter = bt_ctf_iter_create(ctx, NULL, NULL);

    while ((ctf_event = bt_ctf_iter_read_event(iter))) {
        bt_iter_next(bt_ctf_get_iter(iter));
        count++;
    }
    PAPI_read_counters(values, 1);
    instr += values[0];

    clock_gettime(CLOCK_MONOTONIC, &end);
    struct timespec diff = time_diff(start, end);
    double time = (double)diff.tv_sec + ((double)diff.tv_nsec / (double)NSECS_IN_SEC);
    printf("Time : %ld.%lds\n", diff.tv_sec, diff.tv_nsec / NSECS_IN_MSEC);
    printf("Bandwidth : %fMB/s\n", ((double)size/time)/(double)BYTES_IN_MBYTE);
    
    printf("events: %'lu instr/event: %'lu total instr: %'lu\n", count, instr/count, instr);

    return 0;
}
