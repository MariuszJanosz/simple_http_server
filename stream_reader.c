#include <stdlib.h>
#include <stdio.h>
#include <threads.h>
#include <assert.h>
#include <string.h>

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

char* read_line(FILE* stream) {
    static thread_local char leftover[8];
    static thread_local int leftover_len = 0;
    int res_size = 64;
    char* res = (char*)malloc(res_size * sizeof(*res));
    int res_len = 0;
    res[res_len] = '\0';
    while (1) {
        if (leftover_len == 0) { //If leftover is empty read another 8 bytes from the stream
            leftover_len = fread(leftover, 1, 8, stream);
            if (leftover_len < 8) { //error or eof
                if (!feof(stream)) {
                    LOG(ERROR, "fread failed!");
                    exit(1);
                }
            }
        }
        
        //find next \n in leftover
        for (int i = 0; i < leftover_len; ++i) {
            if (leftover[i] == '\n') {
                while (i + res_len + 1 > res_size) { //res would overflow, double it
                    res_size *= 2;
                    char* tmp = (char*)realloc(res, res_size * sizeof(*res));
                    if (!tmp) {
                        LOG(ERROR, "realloc failed!");
                        exit(1);
                    }
                    res = tmp;
                }
                memcpy(res + res_len, leftover, i);
                res_len += i;
                res[res_len] = '\0';
                //shift leftover
                memmove(leftover, leftover + i + 1, leftover_len - i - 1);
                leftover_len -= i + 1;
                return res;
            }
        }

        if (leftover_len > 0) { //if we are here there is no \n in leftover
            while (leftover_len + res_len + 1 > res_size) { //res would overflow, double it
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

        if (feof(stream)) {
            return res;
        }
    }
    assert(0 && "FILE: " __FILE__ " LINE: " stringify(__LINE__) " We should never get here!");
    return res;
}

int stream_reader_thr(void* stream_reader_context) {
    Line_queue_t* line_queue = ((Stream_reader_context_t*)stream_reader_context)->line_queue;
    FILE* stream = ((Stream_reader_context_t*)stream_reader_context)->stream;

    while (!feof(stream)) {
        char* line = read_line(stream);
        
        if (mtx_lock(&line_queue->mtx) == thrd_error) {
            LOG(ERROR, "mtx_lock failed!");
            exit(1);
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
        if (feof(stream)) {
            line_queue->reached_eof = 1;
        }
        cnd_signal(&line_queue->cnd_is_nonempty_or_eof);
        mtx_unlock(&line_queue->mtx);
    }
    thrd_exit(0);
}

