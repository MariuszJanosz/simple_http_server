#ifndef TCP_LISTENER_H
#define TCP_LISTENER_H

#include <stdio.h>
#include <stdint.h>

int create_tcp_listener(uint32_t ip, uint16_t port);
void close_tcp_listener(int tcp_listener_fd);
FILE* accept_tcp_connection(int tcp_listener_fd);

#endif //TCP_LISTENER_H

