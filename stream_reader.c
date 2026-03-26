#include <stdlib.h>
#include <stdio.h>
#include <threads.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>

#include "stream_reader.h"
#include "log.h"

void init_line_queue(Line_queue_t* lq) {
    lq->front = -1;
    lq->rear = -1;
    lq->reached_eof = 0;
    lq->is_nonempty = 0;
    lq->is_nonfull = 1;

    if (mtx_init(&lq->mtx, mtx_plain) == thrd_error) {
        LOG(ERROR, "mtx_init failed!");
        exit(1);
    }
    if (cnd_init(&lq->cnd_is_nonfull) != thrd_success) {
        LOG(ERROR, "cnd_init failed!");
        exit(1);
    }
    if (cnd_init(&lq->cnd_is_nonempty_or_eof) != thrd_success) {
        LOG(ERROR, "cnd_init failed!");
        exit(1);
    }
}

void free_line_queue(Line_queue_t* lq) {
    mtx_destroy(&lq->mtx);
    cnd_destroy(&lq->cnd_is_nonfull);
    cnd_destroy(&lq->cnd_is_nonempty_or_eof);
}

char* read_line(int fd) {
#define LEFTOVER_CAPACITY 8
    static thread_local char leftover[LEFTOVER_CAPACITY];
    static thread_local int leftover_len = 0;
    static thread_local int reached_eof = 0;
    int res_size = 64;
    char* res = (char*)malloc(res_size * sizeof(*res));
    int res_len = 0;
    res[res_len] = '\0';
    while (!reached_eof || leftover_len != 0) {
        //If leftover is empty read another LEFTOVER_CAPACITY bytes from the stream
        if (leftover_len == 0) {
            leftover_len = read(fd, leftover, LEFTOVER_CAPACITY);
            if (leftover_len == 0) {//eof reached
                reached_eof = 1;
            }
            else if (leftover_len == -1) {//error
                LOG(ERROR, "read failed!");
                exit(1);
            }
        }
        
        //find next \n in leftover
        for (int i = 0; i < leftover_len; ++i) {
            if (leftover[i] == '\n') {
                while (res_len + (i + 1) + 1 > res_size) { //res would overflow, double it
                    res_size *= 2;
                    char* tmp = (char*)realloc(res, res_size * sizeof(*res));
                    if (!tmp) {
                        LOG(ERROR, "realloc failed!");
                        exit(1);
                    }
                    res = tmp;
                }
                memcpy(res + res_len, leftover, i + 1);
                res_len += i + 1;
                res[res_len] = '\0';
                //shift leftover
                memmove(leftover, leftover + i + 1, leftover_len - i - 1);
                leftover_len -= i + 1;
                return res;
            }
        }

        if (leftover_len > 0) {//if we are here there is no \n in leftover
            while (res_len + leftover_len + 1 > res_size) {//res would overflow, double it
                res_size *= 2;
                char* tmp = (char*)realloc(res, res_size * sizeof(*res));
                if (!tmp) {
                    LOG(ERROR, "realloc failed!");
                    exit(1);
                }
                res = tmp;
            }
            memcpy(res + res_len, leftover, leftover_len);
            res_len += leftover_len;
            res[res_len] = '\0';
            leftover_len = 0;
        }
    }
    return res;
}

int stream_reader_thr(void* stream_reader_context) {
    Line_queue_t* line_queue = ((Stream_reader_context_t*)stream_reader_context)->line_queue;
    FILE* stream = ((Stream_reader_context_t*)stream_reader_context)->stream;
    int fd = fileno(stream);
    if (fd == -1) {
        LOG(ERROR, "fileno failed!");
        exit(1);
    }

    while (!line_queue->reached_eof) {
        char* line = read_line(fd);
        if (mtx_lock(&line_queue->mtx) == thrd_error) {
            LOG(ERROR, "mtx_lock failed!");
            exit(1);
        }
        if (line[0] == '\0') {
            line_queue->reached_eof = 1;
            free(line);
            cnd_signal(&line_queue->cnd_is_nonempty_or_eof);
            mtx_unlock(&line_queue->mtx);
            continue;
        }
        while (!line_queue->is_nonfull) {
            cnd_wait(&line_queue->cnd_is_nonfull, &line_queue->mtx);
        }
        if (line_queue->front == -1) {
            line_queue->front = 0;
        }
        line_queue->rear += 1;
        line_queue->rear %= LINE_QUEUE_CAPACITY;
        line_queue->queue[line_queue->rear] = line;
        line_queue->is_nonempty = 1;
        if ((line_queue->rear + 1) % LINE_QUEUE_CAPACITY == line_queue->front) {
            line_queue->is_nonfull = 0;
        }
        cnd_signal(&line_queue->cnd_is_nonempty_or_eof);
        mtx_unlock(&line_queue->mtx);
    }
    thrd_exit(0);
}

char* get_line(Line_queue_t* lq) {
    if (mtx_lock(&lq->mtx) == thrd_error) {
        LOG(ERROR, "mtx_lock failed!");
        exit(1);
    }
    while (!lq->is_nonempty) {
        if (lq->reached_eof) {
            return NULL;
        }
        cnd_wait(&lq->cnd_is_nonempty_or_eof, &lq->mtx);
    }
    char* res = lq->queue[lq->front];
    if (lq->front == lq->rear) {
        lq->front = lq->rear = -1;
        lq->is_nonempty = 0;
    }
    else {
        lq->front += 1;
        lq->front %= LINE_QUEUE_CAPACITY;
    }
    lq->is_nonfull = 1;
    cnd_signal(&lq->cnd_is_nonfull);
    mtx_unlock(&lq->mtx);
    return res;
}

int is_reading_finished(Line_queue_t* lq) {
    if (mtx_lock(&lq->mtx) == thrd_error) {
        LOG(ERROR, "mtx_lock failed!");
        exit(1);
    }
    int res = lq->reached_eof && !lq->is_nonempty;
    mtx_unlock(&lq->mtx);
    return res;
}

