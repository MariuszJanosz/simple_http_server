#ifndef STREAM_READER_H
#define STREAM_READER_H

#include <stdio.h>
#include <threads.h>

#define LINE_QUEUE_CAPACITY 64

typedef struct Line_queue_t {
    char* queue[LINE_QUEUE_CAPACITY];
    int front;
    int rear;
    int reached_eof;
    int is_nonfull;
    int is_nonempty;

    mtx_t mtx;
    cnd_t cnd_is_nonfull;
    cnd_t cnd_is_nonempty_or_eof;
} Line_queue_t;

typedef struct Stream_reader_context_t {
    Line_queue_t* line_queue;
    FILE* stream;
} Stream_reader_context_t;

void init_line_queue(Line_queue_t* lq);
void free_line_queue(Line_queue_t* lq);
int stream_reader_thr(void* reader_context);

#endif //STREAM_READER_H

