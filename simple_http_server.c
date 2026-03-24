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
    while (1) { 
        if (mtx_lock(&line_queue.mtx) == thrd_error) {
            LOG(ERROR, "mtx_lock failed!");
            exit(1);
        }
        while (!line_queue.reached_eof && !line_queue.is_nonempty) {
            cnd_wait(&line_queue.cnd_is_nonempty_or_eof, &line_queue.mtx);
        }
        if (line_queue.reached_eof) {
            while (line_queue.is_nonempty) {
                printf("%s\n", line_queue.queue[line_queue.front]);
                if (line_queue.front == line_queue.rear) {
                    line_queue.front = line_queue.rear = -1;
                    line_queue.is_nonempty = 0;
                }
                else {
                    line_queue.front += 1;
                }
            }
            break;
        }
        printf("%s\n", line_queue.queue[line_queue.front]);
        if (line_queue.front == line_queue.rear) {
            line_queue.front = line_queue.rear = -1;
            line_queue.is_nonempty = 0;
        }
        else {
            line_queue.front += 1;
        }
        line_queue.is_nonfull = 1;
        cnd_signal(&line_queue.cnd_is_nonfull);
        mtx_unlock(&line_queue.mtx);
    }

    thrd_join(thr, NULL);
    fclose(stream);
    free_line_queue(&line_queue);
    return 0;
}

