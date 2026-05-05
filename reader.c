#include <stdlib.h>
#include <threads.h>
#include <string.h>
#include <stddef.h>

#include <unistd.h>

#include "reader.h"
#include "log.h"

void init_input_queue(Input_queue_t* iq) {
    iq->front = -1;
    iq->rear = -1;
    iq->reached_eof = 0;
    iq->is_nonempty = 0;
    iq->is_nonfull = 1;

    if (mtx_init(&iq->mtx, mtx_plain) == thrd_error) {
        LOG(ERROR, "mtx_init failed!");
        exit(1);
    }
    if (cnd_init(&iq->cnd_is_nonfull) != thrd_success) {
        LOG(ERROR, "cnd_init failed!");
        exit(1);
    }
    if (cnd_init(&iq->cnd_is_nonempty_or_eof) != thrd_success) {
        LOG(ERROR, "cnd_init failed!");
        exit(1);
    }
}

void free_input_queue(Input_queue_t* iq) {
    mtx_destroy(&iq->mtx);
    cnd_destroy(&iq->cnd_is_nonfull);
    cnd_destroy(&iq->cnd_is_nonempty_or_eof);
    free(iq);
}

int transfer(char* b, int cnt, Input_queue_t* i) {
    if (mtx_lock(&i->mtx) == thrd_error) {
        LOG(ERROR, "mtx_lock failed!");
        exit(1);
    }
    while (!i->is_nonfull) {
        cnd_wait(&i->cnd_is_nonfull, &i->mtx);
    }
    int free_space;
    if (i->front != - 1) {
        free_space = INPUT_QUEUE_CAPACITY -
        (INPUT_QUEUE_CAPACITY + i->rear - i->front + 1) % INPUT_QUEUE_CAPACITY;
    }
    else {
        free_space = INPUT_QUEUE_CAPACITY;
    }
    if (free_space < cnt) {
        cnt = free_space;
    }
    if (i->rear + cnt < INPUT_QUEUE_CAPACITY) {
        memcpy(i->queue + i->rear + 1, b, cnt);
    }
    else {
        int cnt1 = (i->rear + 1 + cnt) % INPUT_QUEUE_CAPACITY;
        memcpy(i->queue + i->rear + 1, b, cnt - cnt1);
        memcpy(i->queue, b + (cnt - cnt1), cnt1);
    }
    i->rear += cnt;
    i->rear %= INPUT_QUEUE_CAPACITY;
    if (i->front == -1) {
        i->front = 0;
    }
    if ((i->rear + 1) % INPUT_QUEUE_CAPACITY == i->front) {
        i->is_nonfull = 0;
    }
    i->is_nonempty = 1;
    cnd_signal(&i->cnd_is_nonempty_or_eof);
    mtx_unlock(&i->mtx);
    return cnt;
}

int read_chunk(int fd, Input_queue_t* iq) {
#define CHUNK_SIZE (4 * 1024)
    char buff[CHUNK_SIZE];
    int cnt = read(fd, buff, CHUNK_SIZE);
    if (cnt == -1) {
        LOG(ERROR, "read failed!");
        exit(1);
    }
    else if (cnt == 0) {
        mtx_lock(&iq->mtx);
        iq->reached_eof = 1;
        cnd_signal(&iq->cnd_is_nonempty_or_eof);
        mtx_unlock(&iq->mtx);
        return 0;
    }
    else {
        char* ptr = buff;
        while (cnt) {
            int tmp = transfer(ptr, cnt, iq);
            cnt -= tmp;
            ptr += tmp;
        }
    }
    return 1;
}

int reader_thr(void* reader_context) {
    Input_queue_t* input_queue = ((Reader_context_t*)reader_context)->input_queue;
    int fd = ((Reader_context_t*)reader_context)->input_fd;

    while (1) {
        if (!read_chunk(fd, input_queue)) {
            break;
        }
    }
    
    free(reader_context);
    thrd_exit(0);
}

int get_data(Input_queue_t* iq, char* output, size_t count) {
    if (mtx_lock(&iq->mtx) == thrd_error) {
        LOG(ERROR, "mtx_lock failed!");
        exit(1);
    }
    while (!iq->is_nonempty) {
        if (iq->reached_eof) {
            mtx_unlock(&iq->mtx);
            return 0;
        }
        cnd_wait(&iq->cnd_is_nonempty_or_eof, &iq->mtx);
    }
    int available_data = (INPUT_QUEUE_CAPACITY + iq->rear - iq->front + 1) %
        INPUT_QUEUE_CAPACITY;
    if (available_data == 0) {
        available_data = INPUT_QUEUE_CAPACITY;
    }
    if (available_data < count) {
        count = available_data;
    }
    if (count <= INPUT_QUEUE_CAPACITY - iq->front) {
        memcpy(output, &iq->queue[iq->front],
                count * sizeof(*output));
    }
    else {
        memcpy(output, &iq->queue[iq->front],
        (INPUT_QUEUE_CAPACITY - iq->front) *
        sizeof(*output));
        memcpy(output +
        (INPUT_QUEUE_CAPACITY - iq->front),
        &iq->queue[0],
        (count -
        (INPUT_QUEUE_CAPACITY - iq->front)) *
        sizeof(*output));
    }
    iq->front += count;
    iq->front %= INPUT_QUEUE_CAPACITY;
    if (count == available_data) {
        iq->front = iq->rear = -1;
        iq->is_nonempty = 0;
    }
    iq->is_nonfull = 1;
    cnd_signal(&iq->cnd_is_nonfull);
    mtx_unlock(&iq->mtx);
    return count;
}

int is_reading_finished(Input_queue_t* iq) {
    if (mtx_lock(&iq->mtx) == thrd_error) {
        LOG(ERROR, "mtx_lock failed!");
        exit(1);
    }
    int res = iq->reached_eof && !iq->is_nonempty;
    mtx_unlock(&iq->mtx);
    return res;
}

Input_queue_t* init_reader(int fd) {
    Input_queue_t* res = malloc(sizeof(*res));
    if (!res) {
        LOG(ERROR, "malloc failed!");
        exit(1);
    }
    init_input_queue(res);
    Reader_context_t* src = malloc(sizeof(*src));
    if (!src) {
        LOG(ERROR, "malloc failed!");
        exit(1);
    }
    src->input_queue = res;
    src->input_fd = fd;
    thrd_t thr;
    thrd_create(&thr, reader_thr, src);
    thrd_detach(thr);
    return res;
}

char* input_queue_get_line(Input_queue_t* iq, int* new_line_found) {
    if (mtx_lock(&iq->mtx) == thrd_error) {
        LOG(ERROR, "mtx_lock failed!");
        exit(1);
    }
    while (!iq->is_nonempty) {
        if (iq->reached_eof) {
            mtx_unlock(&iq->mtx);
            return NULL;
        }
        cnd_wait(&iq->cnd_is_nonempty_or_eof, &iq->mtx);
    }
    int i = iq->front;
    while (iq->queue[i] != '\n' && i != iq->rear) {
        i = (i + 1) % INPUT_QUEUE_CAPACITY;
    }
    if (iq->queue[i] == '\n') {
        *new_line_found = 1;
    }
    else {
        *new_line_found = 0;
    }
    int size = (INPUT_QUEUE_CAPACITY + i - iq->front + 1) % INPUT_QUEUE_CAPACITY;
    if (size == 0) {
        size == INPUT_QUEUE_CAPACITY;
    }
    mtx_unlock(&iq->mtx);
    char* res = malloc((size + 1) * sizeof(*res));
    if (!res) {
        LOG(ERROR, "malloc failed!");
        exit(1);
    }
    res[size] = '\0';
    char* ptr = res;
    while(size) {
        int tmp = get_data(iq, ptr, size);
        ptr += tmp;
        size -= tmp;
    }
    return res;
}

