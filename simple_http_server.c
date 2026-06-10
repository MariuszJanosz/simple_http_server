#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <limits.h>
#include <stdint.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>

#include "log.h"
#include "tcp_connection.h"
#include "http_request_queue.h"
#include "http_resources.h"

int g_workers_finished = 0;
cnd_t g_cnd_worker_finished;
char g_www_root[PATH_MAX];
size_t g_root_len;

struct in_addr addr;
uint16_t port = 0;

void process_command_line_arguments(int argc, char** argv) {
    if (argc != 4) {
        printf("USAGE: simple_http_server <path_to_www_root> <ip> <port>\n");
        exit(1);
    }
    
    if (!realpath(argv[1], g_www_root)) {
        printf("Invalid path to www root.\n");
        printf("USAGE: simple_http_server <path_to_www_root> <ip> <port>\n");
        exit(1);
    }

    g_root_len = strlen(g_www_root);

    if (!inet_aton(argv[2], &addr)) {
        printf("Invalid ip.\n");
        printf("USAGE: simple_http_server <path_to_www_root> <ip> <port>\n");
        exit(1);
    }

    char* ptr = argv[3];
    while (*ptr) {
        if ('0' <= *ptr && *ptr <= '9') {
            int digit = *ptr - '0';
            if (port > (UINT16_MAX - digit) / 10) {
                printf("Invalid port.\n");
                printf("USAGE: simple_http_server <path_to_www_root> <ip> <port>\n");
                exit(1);
            }
            port = 10 * port + digit;
        }
        else {
            printf("Invalid port.\n");
            printf("USAGE: simple_http_server <path_to_www_root> <ip> <port>\n");
            exit(1);
        }
        ++ptr;
    }
    port = htons(port);
}

int main(int argc, char** argv) {
    process_command_line_arguments(argc, argv);
    init_resources_from_config();
    //Do not wait for children return values, prevents zombie processes
    signal(SIGCHLD, SIG_IGN);

    //Listen for tcp connection
    int tcp_listener_fd = create_tcp_listener(addr.s_addr, port);
    Tcp_connection_t tcp_connection;
    while (1) {
        tcp_connection = accept_tcp_connection(tcp_listener_fd);
        if (fork()) { //It should be a thread pool instead of fork but ...
            close_tcp_connection(&tcp_connection);
            continue; //Parent keeps listening
        }
        else {
            close_tcp_listener(tcp_listener_fd);
            break; //Child deals with the established connection
        }
    }

    Request_queue_t rq;
    init_request_queue(&rq);
    if (cnd_init(&g_cnd_worker_finished) != thrd_success) {
        LOG(ERROR, "cnd_init failed!");
        exit(1);
    }

    //Start worker threads writing and sending responses
#define NUMBER_OF_WRITERS 4
    if (NUMBER_OF_WRITERS > REQUEST_QUEUE_CAPACITY) {
        LOG(ERROR, "Too many writers!");
        exit(1);
    }
    init_writers(&rq, NUMBER_OF_WRITERS, tcp_connection);

    //Loop inside request queue menager and parse requests as long as the connection is active
    request_queue_manager(&rq, tcp_connection);

    //Wait for all workers to finish
    if (mtx_lock(&rq.mtx) == thrd_error) {
        LOG(ERROR, "mtx_lock failed!");
        exit(1);
    }
    while (g_workers_finished < NUMBER_OF_WRITERS) {
        cnd_wait(&g_cnd_worker_finished, &rq.mtx);
    }
    mtx_unlock(&rq.mtx);

    //Cleanup
    cnd_destroy(&g_cnd_worker_finished);
    free_request_queue(&rq);
    close_tcp_connection(&tcp_connection);
    fflush(stdout);
    return 0;
}

