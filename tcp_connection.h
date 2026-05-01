#ifndef TCP_CONNECTION_H
#define TCP_CONNECTION_H

#include <stdint.h>

typedef struct Tcp_connection_t {
    int fd;
} Tcp_connection_t;

int create_tcp_listener(uint32_t ip, uint16_t port);
void close_tcp_listener(int tcp_listener_fd);
Tcp_connection_t accept_tcp_connection(int tcp_listener_fd);
void close_tcp_connection(Tcp_connection_t* tcp_connection);

#endif //TCP_CONNECTION_H

