#include "log.h"
#include "http_request_queue.h"
#include "tcp_connection.h"
#include "http_message.h"
#include "reader.h"
#include "http_request_handler.h"

#include <threads.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <signal.h>

extern int workers_finished;
extern cnd_t cnd_worker_finished;
int request_queue_manager_finished = 0;

void echo_request(Http_message_t* req) {
    static int req_id = 0;
    Request_line_t* req_line = (Request_line_t*)req->start_line;
    printf("---req---echo---id:%05d---\n", req_id++);
    if (!req_line) {
        printf("Missing reqest line!\n\n");
        fflush(stdout);
        return;
    }
    printf("%s %s %s\n",
        http_method_to_string(req_line->method),
        req_line->request_target,
        req_line->http_version);
    for (int i = 0; i < req->field_lines_count; ++i) {
        printf("%s: %s\n", req->field_lines[i].field_name, req->field_lines[i].field_value);
    }
    printf("\n");
    if (req->message_body) {
        if (req->body_size <= INT_MAX) {
            printf("%.*s\n", (int)req->body_size, req->message_body);
        }
        else {
            LOG(INFO, "Body too big, printf skipped!");
        }
        printf("\n");
    }
    fflush(stdout);
}

void echo_response(Http_message_t* res) {
    static int res_id = 0;
    Status_line_t* status_line = (Status_line_t*)res->start_line;
    printf("---res---echo---id:%05d---\n", res_id++);
    printf("%s %s %s\n",
        status_line->http_version,
        status_line->status_code,
        status_line->status_text);
    for (int i = 0; i < res->field_lines_count; ++i) {
        printf("%s: %s\n", res->field_lines[i].field_name, res->field_lines[i].field_value);
    }
    printf("\n");
    if (res->message_body) {
        if (res->body_size <= INT_MAX) {
            printf("%.*s\n", (int)res->body_size, res->message_body);
        }
        else {
            LOG(INFO, "Body too big, printf skipped!");
        }
        printf("\n");
    }
    fflush(stdout);
}

void init_request_queue(Request_queue_t* rq) {
    for (int i = 0; i < REQUEST_QUEUE_CAPACITY; ++i) {
        Request_block_t* rb = &rq->queue[i];
        rb->request_ready = 0;
        if (cnd_init(&rb->cnd_request_ready) != thrd_success) {
            LOG(ERROR, "cnd_init failed!");
            exit(1);
        }
        if (cnd_init(&rb->cnd_is_front) != thrd_success) {
            LOG(ERROR, "cnd_init failed!");
            exit(1);
        }
    }
    rq->front = 0;
    rq->rear = -1;
    rq->is_nonfull = 1;

    if (mtx_init(&rq->mtx, mtx_plain) == thrd_error) {
        LOG(ERROR, "mtx_init failed!");
        exit(1);
    }
    if (cnd_init(&rq->cnd_is_nonfull) != thrd_success) {
        LOG(ERROR, "cnd_init failed!");
        exit(1);
    }
}

void free_request_queue(Request_queue_t* rq) {
    for (int i = 0; i < REQUEST_QUEUE_CAPACITY; ++i) {
        Request_block_t* rb = &rq->queue[i];
        cnd_destroy(&rb->cnd_request_ready);
        cnd_destroy(&rb->cnd_is_front);
    }
    mtx_destroy(&rq->mtx);
    cnd_destroy(&rq->cnd_is_nonfull);
}

int response_writer_thr(void* response_writer_context) {
    //Ignore signal if the other side closes connection
    signal(SIGPIPE, SIG_IGN);

    Request_queue_t* rq = ((Response_writer_context_t*)(response_writer_context))->rq;
    int number_of_workers = ((Response_writer_context_t*)(response_writer_context))->number_of_workers;
    int curr_index = ((Response_writer_context_t*)(response_writer_context))->start_index;
    Tcp_connection_t tcp_con = ((Response_writer_context_t*)(response_writer_context))->tcp_con;
    
    while (1) {
        //Wait until request is ready
        if (mtx_lock(&rq->mtx) == thrd_error) {
            LOG(ERROR, "mtx_lock failed!");
            exit(1);
        }
        Request_block_t* rb = &rq->queue[curr_index];
        while (!rb->request_ready) {
            if (request_queue_manager_finished && !rq->queue[curr_index].request_ready) {
                goto cleanup;
            }
            cnd_wait(&rb->cnd_request_ready, &rq->mtx);
        }
        rb->request_ready = 0;
        Http_message_t *req = rb->req;
        Http_status_t status = rb->status;
        mtx_unlock(&rq->mtx);

        //Prepare response
        Http_message_t res;
        init_http_message(&res, HTTP_RESPONSE);
        handle_http_request(req, &res, &status);
        
        //Wait until it is this response turn
        if (mtx_lock(&rq->mtx) == thrd_error) {
            LOG(ERROR, "mtx_lock failed!");
            exit(1);
        }
        while (curr_index != rq->front) {
            cnd_wait(&rb->cnd_is_front, &rq->mtx);
        }

        //Send response
        send_response(tcp_con, &res);

        DEBUG(echo_response(&res));

        //Request block cleanup
        if (req) {
            free_http_message(req);
            free(req);
        }
        free_http_message(&res);
        rq->front += 1;
        rq->front %= REQUEST_QUEUE_CAPACITY;
        rq->is_nonfull = 1;
        //Move to next queue position
        curr_index += number_of_workers;
        curr_index %= REQUEST_QUEUE_CAPACITY;
        //Wake next writer if ready and queue manager if queue was full
        cnd_signal(&rq->queue[rq->front].cnd_is_front);
        cnd_signal(&rq->cnd_is_nonfull);
        if (request_queue_manager_finished && !rq->queue[curr_index].request_ready) {
            goto cleanup;
        }
        mtx_unlock(&rq->mtx);
    }

    //Cleanup
cleanup:
    free(response_writer_context);
    workers_finished += 1;
    cnd_signal(&cnd_worker_finished);
    mtx_unlock(&rq->mtx);
    thrd_exit(0);
}

void init_writers(Request_queue_t* rq, int number_of_workers, Tcp_connection_t tcp_con) {
    for (int i = 0; i < number_of_workers; ++i) {
        Response_writer_context_t* rwc = malloc(sizeof(*rwc));
        if (!rwc) {
            LOG(ERROR, "malloc failed!");
            exit(1);
        }
        rwc->rq = rq;
        rwc->number_of_workers = number_of_workers;
        rwc->start_index = i;
        rwc->tcp_con = tcp_con;

        thrd_t thr;
        thrd_create(&thr, response_writer_thr, rwc);
        thrd_detach(thr);
    }
}

void request_queue_manager(Request_queue_t* rq, Tcp_connection_t tcp_con) {
    while (!is_reading_finished(tcp_con)) {
        //prepare next request
        Http_message_t* req = malloc(sizeof(*req));
        if (!req) {
            LOG(ERROR, "malloc failed!");
            exit(1);
        }
        init_http_message(req, HTTP_REQUEST);
        Http_status_t status = parse_http_request(req, tcp_con);
        
        DEBUG(echo_request(req));

        //Get rq->mtx and put request to rq
        if (mtx_lock(&rq->mtx) == thrd_error) {
            LOG(ERROR, "mtx_lock failed!");
            exit(1);
        }

        //If request is broken break
        if (!req->start_line) {
            free(req);
            abort_reading(tcp_con);
            mtx_unlock(&rq->mtx);
            break;
        }

        while (!rq->is_nonfull) {
            cnd_wait(&rq->cnd_is_nonfull, &rq->mtx);
        }
        rq->rear += 1;
        rq->rear %= REQUEST_QUEUE_CAPACITY;
        if ((rq->rear + 1) % REQUEST_QUEUE_CAPACITY == rq->front) {
            rq->is_nonfull = 0;
        }
        Request_block_t* rb = &rq->queue[rq->rear];
        rb->req = req;
        rb->status = status;
        rb->request_ready = 1;
        cnd_signal(&rb->cnd_request_ready);
        mtx_unlock(&rq->mtx);
    }
    if (mtx_lock(&rq->mtx) == thrd_error) {
        LOG(ERROR, "mtx_lock failed!");
        exit(1);
    }
    request_queue_manager_finished = 1;
    for (int i = 0; i < REQUEST_QUEUE_CAPACITY; ++i) {
        cnd_signal(&rq->queue[i].cnd_request_ready);
    }
    mtx_unlock(&rq->mtx);
}

