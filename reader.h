#ifndef READER_H
#define READER_H

#include "tcp_connection.h"

#include <stddef.h>

size_t get_data(Tcp_connection_t tcp_con, char* output, size_t count);
char* get_line(Tcp_connection_t tcp_con);
int is_reading_finished(Tcp_connection_t tcp_con);
void abort_reading(Tcp_connection_t tcp_con);

#endif //READER_H

