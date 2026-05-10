#include "reader.h"
#include "log.h"

#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#include <unistd.h>

#define LEFTOVER_CAPACITY (4 * 1024)

int reading_finished = 0;
char leftover[LEFTOVER_CAPACITY];
int leftover_size = 0;
int leftover_start_index = 0;

size_t try_get_data(Tcp_connection_t tcp_con, char* output, size_t count) {
    size_t r = read(tcp_con.fd, output, count);
    if (r < 0) {
        LOG(ERROR, "read faile!");
        exit(1);
    }
    return r;
}

size_t get_data(Tcp_connection_t tcp_con, char* output, size_t count) {
    if (count <= leftover_size) {
        memcpy(output, leftover + leftover_start_index, count);
        leftover_start_index += count;
        leftover_size -= count;
        return count;
    }
    else if (leftover_size > 0) {
        memcpy(output, leftover, leftover_size);
        leftover_start_index = 0;
        leftover_size = 0;
        return leftover_size;
    }
    leftover_size = try_get_data(tcp_con, leftover, LEFTOVER_CAPACITY);
    if (leftover_size == 0) {//EOF
        return 0;
    }
    return get_data(tcp_con, output, count);
}

char* get_line(Tcp_connection_t tcp_con) {
    if (leftover_size > 0) {
        int i = 0;
        while (i < leftover_size) {
            if (leftover[leftover_start_index + i] == '\n')
                break;
            ++i;
        }
        //If there is '\n' in leftover
        if (i < leftover_size) {
            char* res = malloc((i + 2)); // (i + 1) == number of characters, +1 for '\0'
            if (!res) {
                LOG(ERROR, "malloc failed!");
                exit(1);
            }
            memcpy(res, leftover + leftover_start_index, i + 1);
            res[i + 1] = '\0';
            leftover_start_index += (i + 1);
            leftover_size -= (i + 1);
            return res;
        }
        //There is no '\n'
        else {
            char* res = malloc((leftover_size + 1));
            if (!res) {
                LOG(ERROR, "malloc failed!");
                exit(1);
            }
            memcpy(res, leftover + leftover_start_index, leftover_size);
            res[leftover_size] = '\0';
            leftover_start_index = 0;
            leftover_size = 0;
            char* tail = get_line(tcp_con);
            if (!tail) { //EOF return whatever is already in res
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
            return res;
        }
    }
    leftover_size = try_get_data(tcp_con, leftover, LEFTOVER_CAPACITY);
    //EOF detected return NULL
    if (leftover_size == 0) {
        return NULL;
    }
    return get_line(tcp_con);
}

int is_reading_finished(Tcp_connection_t tcp_con) {
    return reading_finished;
}

void abort_reading(Tcp_connection_t tcp_con) {
    reading_finished = 1;
}

