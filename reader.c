#include "reader.h"
#include "log.h"

#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <threads.h>

#include <unistd.h>
#include <poll.h>

#define LEFTOVER_CAPACITY (4 * 1024)

static thread_local int stl_reading_finished = 0;
static thread_local char stl_leftover[LEFTOVER_CAPACITY];
static thread_local int stl_leftover_size = 0;
static thread_local int stl_leftover_start_index = 0;

size_t try_get_data(Tcp_connection_t tcp_con, char* output, size_t count) {
    if (stl_reading_finished) return 0;
    int pollres;
    struct pollfd fds;
    fds.fd = tcp_con.fd;
    fds.events = POLLIN;
#define TIMEOUT (60 * 1000) //60 x 1000 ms == 1 min
    pollres = poll(&fds, 1, TIMEOUT);
    if (pollres < 0) {
        LOG(ERROR, "poll failed!");
        exit(1);
    }
    else if (pollres == 0) {
        //Timeout close connection
        stl_reading_finished = 1;
        return 0;
    }
    ssize_t r = read(tcp_con.fd, output, count);
    if (r < 0) {
        LOG(ERROR, "read faile!");
        exit(1);
    }
    return r;
}

size_t get_data(Tcp_connection_t tcp_con, char* output, size_t count) {
    if (count <= stl_leftover_size) {
        memcpy(output, stl_leftover + stl_leftover_start_index, count);
        stl_leftover_start_index += count;
        stl_leftover_size -= count;
        return count;
    }
    else if (stl_leftover_size > 0) {
        memcpy(output, stl_leftover, stl_leftover_size);
        stl_leftover_start_index = 0;
        count = stl_leftover_size;
        stl_leftover_size = 0;
        return count;
    }
    stl_leftover_start_index = 0;
    stl_leftover_size = try_get_data(tcp_con, stl_leftover, LEFTOVER_CAPACITY);
    if (stl_leftover_size == 0) {//EOF
        return 0;
    }
    return get_data(tcp_con, output, count);
}

char* get_line(Tcp_connection_t tcp_con) {
    static int recur_depth = 0;
    ++recur_depth;
#define MAX_RECUR_DEPTH 20
    //This prevents the attack of sending infinite line that would cause stack overflow
    if (recur_depth > MAX_RECUR_DEPTH) return NULL;
    if (stl_leftover_size > 0) {
        int i = 0;
        while (i < stl_leftover_size) {
            if (stl_leftover[stl_leftover_start_index + i] == '\n')
                break;
            ++i;
        }
        //If there is '\n' in leftover
        if (i < stl_leftover_size) {
            char* res = malloc((i + 2)); // (i + 1) == number of characters, +1 for '\0'
            if (!res) {
                LOG(ERROR, "malloc failed!");
                exit(1);
            }
            memcpy(res, stl_leftover + stl_leftover_start_index, i + 1);
            res[i + 1] = '\0';
            stl_leftover_start_index += (i + 1);
            stl_leftover_size -= (i + 1);
            --recur_depth;
            return res;
        }
        //There is no '\n'
        else {
            char* res = malloc((stl_leftover_size + 1));
            if (!res) {
                LOG(ERROR, "malloc failed!");
                exit(1);
            }
            memcpy(res, stl_leftover + stl_leftover_start_index, stl_leftover_size);
            res[stl_leftover_size] = '\0';
            stl_leftover_start_index = 0;
            stl_leftover_size = 0;
            ++recur_depth;
            char* tail = get_line(tcp_con);
            if (!tail) { //EOF return whatever is already in res
                --recur_depth;
                return res;
            }
            size_t rsize = strlen(res);
            size_t tsize = strlen(tail);
            char* tmp = realloc(res, rsize + tsize + 1);
            if (!tmp) {
                LOG(ERROR, "realloc failed!");
                exit(1);
            }
            res = tmp;
            memcpy(res + rsize, tail, tsize + 1);
            free(tail);
            --recur_depth;
            return res;
        }
    }
    stl_leftover_start_index = 0;
    stl_leftover_size = try_get_data(tcp_con, stl_leftover, LEFTOVER_CAPACITY);
    //EOF detected return NULL
    if (stl_leftover_size == 0) {
        --recur_depth;
        return NULL;
    }
    --recur_depth;
    return get_line(tcp_con);
}

int is_reading_finished(Tcp_connection_t tcp_con) {
    return stl_reading_finished;
}

void abort_reading(Tcp_connection_t tcp_con) {
    stl_reading_finished = 1;
}

