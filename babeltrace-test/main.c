#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <locale.h>
#include <papi.h>
#include <pthread.h>

#include <babeltrace/babeltrace.h>
#include <babeltrace/iterator.h>
#include <babeltrace/ctf/iterator.h>
#include <babeltrace/ctf/types.h>

uint64_t count = 0;
uint64_t instr = 0;

void seek(struct bt_stream_pos *pos, size_t index, int whence) {
    static uint64_t last_count = 0;
    struct ctf_stream_pos *p = ctf_pos(pos);
    long long int values[1] = {0};
    PAPI_read_counters(values, 1);
    instr += values[0];
    /*printf("packet size:%'lu events:%'lu instr:%'lld\n", p->content_size, count - last_count, values[0]);*/
    last_count = count;
    ctf_packet_seek(pos, index, whence);
}

int main(int argc, char **argv) {
    struct bt_context *ctx;
    struct bt_ctf_iter *iter;
    struct bt_ctf_event *ctf_event;
    char *trace_path;
    int trace_id;

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

    printf("events: %'lu instr/event: %'lu total instr: %'lu\n", count, instr/count, instr);

    return 0;
}
