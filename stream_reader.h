#ifndef STREAM_READER_H
#define STREAM_READER_H

#include <stdio.h>
#include <threads.h>

#define INPUT_QUEUE_CAPACITY (64 * 1024)

typedef struct Input_queue_t {
    char* queue[INPUT_QUEUE_CAPACITY];
    int front;
    int rear;
    int reached_eof;
    int is_nonfull;
    int is_nonempty;

    mtx_t mtx;
    cnd_t cnd_is_nonfull;
    cnd_t cnd_is_nonempty_or_eof;
} Input_queue_t;

typedef struct Stream_reader_context_t {
    Input_queue_t* input_queue;
    FILE* stream;
} Stream_reader_context_t;

void init_input_queue(Input_queue_t* iq);
void free_input_queue(Input_queue_t* iq);
int stream_reader_thr(void* reader_context);
int get_data(Input_queue_t* iq, char* output, int count);
int is_reading_finished(Input_queue_t* iq);
Input_queue_t* init_stream_reader(FILE* stream); 

#endif //STREAM_READER_H

