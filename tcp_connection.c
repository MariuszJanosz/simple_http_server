#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "tcp_connection.h"
#include "log.h"

#define CONN_BACKLOG 8

int create_tcp_listener(uint32_t ip, uint16_t port) {
    int list_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (list_sock_fd == -1) {
        LOG(ERROR, "socket failed!");
        exit(1);
    }
    int a = 1;
    if (setsockopt(list_sock_fd, SOL_SOCKET, SO_REUSEADDR, &a, sizeof(a)) != 0) {
        LOG(ERROR, "setsockopt failed!");
        exit(1);
    }
    struct sockaddr_in list_sock_addr;
    memset(&list_sock_addr, 0, sizeof(list_sock_addr));
    list_sock_addr.sin_family = AF_INET;
    list_sock_addr.sin_port = htons(port);
    list_sock_addr.sin_addr.s_addr = htonl(ip);
    if (bind(list_sock_fd, (const struct sockaddr*)&list_sock_addr,
                sizeof(list_sock_addr)) == -1) {
        LOG(ERROR, "bind failed!");
        close(list_sock_fd);
        exit(1);
    }
    if (listen(list_sock_fd, CONN_BACKLOG) == -1) {
        LOG(ERROR, "listen failed!");
        close(list_sock_fd);
        exit(1);
    }
    return list_sock_fd;
}

void close_tcp_listener(int tcp_listener_fd) {
    close(tcp_listener_fd);
}

Tcp_connection_t accept_tcp_connection(int tcp_listener_fd) {
    Tcp_connection_t res;
    res.fd = accept(tcp_listener_fd, NULL, NULL);
    if (res.fd == -1) {
        LOG(ERROR, "accept failed!");
        exit(1);
    }
    return res;
}

void close_tcp_connection(Tcp_connection_t* tcp_connection) {
   close(tcp_connection->fd); 
}

