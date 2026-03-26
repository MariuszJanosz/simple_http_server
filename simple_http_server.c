#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <threads.h>

#include <unistd.h>

#include "log.h"
#include "stream_reader.h"
#include "tcp_listener.h"

#define IP ((unsigned int)127*(1<<24))+(0*(1<<16))+(0*(1<<8))+1 //127.0.0.1 localhost
#define PORT 54321

int main() {
    //Listen for tcp connections, if connection established prepare in and out streams
    int tcp_listener_fd = create_tcp_listener(IP, PORT);
    FILE* in_stream = accept_tcp_connection(tcp_listener_fd);
    if (!in_stream) {
        LOG(ERROR, "accept_tcp_connection failed!");
        exit(1);
    }
    int fd = fileno(in_stream);
    if (fd == -1) {
        LOG(ERROR, "fileno failed!");
        exit(1);
    }
    FILE* out_stream = fdopen(fd, "w");
    if (!out_stream) {
        LOG(ERROR, "fdopen failed!");
        exit(1);
    }

    //Create in_stream reader
    Line_queue_t line_queue;
    init_line_queue(&line_queue);
    Stream_reader_context_t src;
    src.line_queue = &line_queue;
    src.stream = in_stream;
    thrd_t thr;
    thrd_create(&thr, stream_reader_thr, &src);

    //Read incoming data line by line
    while (!is_reading_finished(&line_queue)) {
        char* line = get_line(&line_queue);
        if (!line) {
            break;
        }
        printf("%s", line);
        fprintf(out_stream, "Server echo: %s", line);
        free(line);
        fflush(out_stream);
    }

    thrd_join(thr, NULL);

    fclose(in_stream);
    fclose(out_stream);
    free_line_queue(&line_queue);
    fflush(stdout);
    return 0;
}

