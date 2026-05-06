#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <limits.h>

#include <unistd.h>

#include "log.h"
#include "reader.h"
#include "tcp_connection.h"
#include "http_request_queue.h"

#define IP (((unsigned int)127*(1<<24))+(0*(1<<16))+(0*(1<<8))+1) //127.0.0.1 localhost
#define PORT 54321

int workers_finished = 0;
cnd_t cnd_worker_finished;

void set_up_www_root(int argc, char** argv, char* www_root) {
    if (argc != 2) {
        printf("USAGE: simple_http_server <path_to_www_root>\n");
        exit(1);
    }
    
    if (!realpath(argv[1], www_root)) {
        printf("USAGE: simple_http_server <path_to_www_root>\n");
        exit(1);
    }
}

int main(int argc, char** argv) {
    char www_root[PATH_MAX];
    set_up_www_root(argc, argv, www_root);
    //Do not wait for children return values, prevents zombie processes
    signal(SIGCHLD, SIG_IGN);

    //Listen for tcp connection
    int tcp_listener_fd = create_tcp_listener(IP, PORT);
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

    Input_queue_t* iq = init_reader(tcp_connection.fd);
    Request_queue_t rq;
    init_request_queue(&rq);
    if (cnd_init(&cnd_worker_finished) != thrd_success) {
        LOG(ERROR, "cnd_init failed!");
        exit(1);
    }

    //Start worker threads writing and sending responses
#define NUMBER_OF_WRITERS 4
    if (NUMBER_OF_WRITERS > REQUEST_QUEUE_CAPACITY) {
        LOG(ERROR, "Too many writers!");
        exit(1);
    }
    init_writers(&rq, NUMBER_OF_WRITERS, tcp_connection, www_root);

    //Loop inside request queue menager and parse requests as long as the connection is active
    request_queue_manager(&rq, iq);

    //Wait for all workers to finish
    if (mtx_lock(&rq.mtx) == thrd_error) {
        LOG(ERROR, "mtx_lock failed!");
        exit(1);
    }
    while (workers_finished < NUMBER_OF_WRITERS) {
        cnd_wait(&cnd_worker_finished, &rq.mtx);
    }
    mtx_unlock(&rq.mtx);

    //Cleanup
    cnd_destroy(&cnd_worker_finished);
    free_request_queue(&rq);
    close_tcp_connection(&tcp_connection);
    free_input_queue(iq);
    fflush(stdout);
    return 0;
}

