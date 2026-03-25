#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <threads.h>

#include "log.h"
#include "stream_reader.h"
#include "tcp_listener.h"

#define IP (127 * (1 << 24) + 1) //127.0.0.1 localhost
#define PORT 54321

int main() {
    int tcp_listener_fd = create_tcp_listener(IP, PORT);
    FILE* stream = accept_tcp_connection(tcp_listener_fd);
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
    close_tcp_listener(tcp_listener_fd);
    fflush(stdout);
    return 0;
}

