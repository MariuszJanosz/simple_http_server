#ifndef READER_H
#define READER_H

#include <stdio.h>
#include <threads.h>
#include <inttypes.h>

#define INPUT_QUEUE_CAPACITY (64 * 1024)

typedef struct Input_queue_t {
    char queue[INPUT_QUEUE_CAPACITY];
    int front;
    int rear;
    int reached_eof;
    int is_nonfull;
    int is_nonempty;

    mtx_t mtx;
    cnd_t cnd_is_nonfull;
    cnd_t cnd_is_nonempty_or_eof;
} Input_queue_t;

typedef struct Reader_context_t {
    Input_queue_t* input_queue;
    int input_fd;
} Reader_context_t;

void init_input_queue(Input_queue_t* iq);
void free_input_queue(Input_queue_t* iq);
int reader_thr(void* reader_context);
int get_data(Input_queue_t* iq, char* output, intmax_t count);
int is_reading_finished(Input_queue_t* iq);
Input_queue_t* init_reader(int fd); 
char* input_queue_get_line(Input_queue_t* iq, int* new_line_found);

#endif //READER_H

