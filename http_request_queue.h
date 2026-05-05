#ifndef HTTP_REQUEST_QUEUE_H
#define HTTP_REQUEST_QUEUE_H

#include "http_message.h"
#include "reader.h"
#include "tcp_connection.h"

#include <threads.h>

#define REQUEST_QUEUE_CAPACITY 256

typedef struct Request_response_pair_t {
    Http_message_t* req;
    Http_status_t status;
    Http_message_t* res;
    int request_ready;

    cnd_t cnd_request_ready;
    cnd_t cnd_is_front;
} Request_response_pair_t;

typedef struct Request_queue_t {
    Request_response_pair_t queue[REQUEST_QUEUE_CAPACITY];
    int front;
    int rear;
    int is_nonfull;
    int is_empty;

    mtx_t mtx;

    cnd_t cnd_is_nonfull;
} Request_queue_t;

typedef struct Response_writer_context_t {
    Request_queue_t* rq;
    int number_of_workers;
    int start_index;
    Tcp_connection_t tcp_con;
    char* www_root;
} Response_writer_context_t;

void init_request_queue(Request_queue_t* rq);
void free_request_queue(Request_queue_t* rq);
int response_writer_thr(void* response_writer_context);
void init_writers(Request_queue_t* rq, int number_of_workers, Tcp_connection_t tcp_con, char* www_root);
void request_queue_manager(Request_queue_t* rq, Input_queue_t* iq);

#endif //HTTP_REQUEST_QUEUE_H

