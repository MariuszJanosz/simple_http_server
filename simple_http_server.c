#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <threads.h>

#include "log.h"
#include "stream_reader.h"

int main() {
    FILE* stream = fopen("simple_http_server.c", "rb");
    if (!stream) {
        LOG(ERROR, "fopen failed!");
        exit(1);
    }
    Line_queue_t line_queue;
    init_line_queue(&line_queue);
    Stream_reader_context_t src;
    src.line_queue = &line_queue;
    src.stream = stream;
    thrd_t thr;
    thrd_create(&thr, stream_reader_thr, &src);

    while (!is_reading_finished(&line_queue)) {
        char* line = get_line(&line_queue);
        printf("%s\n", line);
        free(line);
    }

    thrd_join(thr, NULL);
    fclose(stream);
    free_line_queue(&line_queue);
    return 0;
}

