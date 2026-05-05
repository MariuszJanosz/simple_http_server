#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <threads.h>
#include <signal.h>
#include <stdint.h>
#include <limits.h>

#include <unistd.h>

#include "log.h"
#include "reader.h"
#include "tcp_connection.h"
#include "http_message.h"
#include "http_routing.h"
#include "http_request_queue.h"

#define IP (((unsigned int)127*(1<<24))+(0*(1<<16))+(0*(1<<8))+1) //127.0.0.1 localhost
#define PORT 54321

typedef struct Http_server_t {
    int tcp_listener_fd;
    Tcp_connection_t tcp_connection;
    Input_queue_t* input_queue;
} Http_server_t;

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("USAGE: simple_http_server <path_to_www_root>\n");
        return 1;
    }
    
    char www_root[PATH_MAX];
    if (!realpath(argv[1], www_root)) {
        printf("USAGE: simple_http_server <path_to_www_root>\n");
        return 1;
    }

    signal(SIGCHLD, SIG_IGN);
    
    Http_server_t s;

    //Listen for tcp connection
    s.tcp_listener_fd = create_tcp_listener(IP, PORT);
    while (1) {
        s.tcp_connection = accept_tcp_connection(s.tcp_listener_fd);
        if (fork()) { //It should be a thread pool instead of fork but ...
            close_tcp_connection(&s.tcp_connection);
            continue; //Parent keeps listening
        }
        else {
            close_tcp_listener(s.tcp_listener_fd);
            break; //Child deals with the established connection
        }
    }
    
    //Create reader
    s.input_queue = init_reader(s.tcp_connection.fd);
    
    Request_queue_t rq;
    init_request_queue(&rq);

    int number_of_writers = 8;
    if (number_of_writers > REQUEST_QUEUE_CAPACITY) {
        LOG(ERROR, "Too many writers!");
        exit(1);
    }
    init_writers(&rq, number_of_writers, s.tcp_connection, www_root);

    //Loop inside request queue menager as long as the connection is active
    request_queue_manager(&rq, s.input_queue);

    free_request_queue(&rq);
    close_tcp_connection(&s.tcp_connection);
    free_input_queue(s.input_queue);
    fflush(stdout);
    return 0;
}

