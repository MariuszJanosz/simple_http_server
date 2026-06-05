#include "log.h"
#include "http_request_queue.h"
#include "http_request_context.h"
#include "http_response.h"

#include <threads.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

extern int g_workers_finished;
extern cnd_t g_cnd_worker_finished;
static int s_request_queue_manager_finished = 0;

void echo_request_response_pair(Http_request_context_t* req_con, Http_response_t* res) {
    printf("---request----\n");
    print_request_context(req_con);
    printf("---response---\n");
    print_response(res);
}

void init_request_queue(Request_queue_t* rq) {
    for (int i = 0; i < REQUEST_QUEUE_CAPACITY; ++i) {
        Request_block_t* rb = &rq->queue[i];
        init_request_context(&rb->req_con);
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
        free_request_context(&rb->req_con);
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
            if (s_request_queue_manager_finished && !rq->queue[curr_index].request_ready) {
                goto cleanup;
            }
            cnd_wait(&rb->cnd_request_ready, &rq->mtx);
        }
        rb->request_ready = 0;
        Http_request_context_t* req_con = &rb->req_con;
        mtx_unlock(&rq->mtx);

        //Process request to fill req_con or return immediately if PARSING_BROKEN_...
        req_con->status == process_request(req_con);
        if (req_con->close_connection_after_response) {
            abort_reading(tcp_con);
        }

        //Prepare response based on request context, or generic one in case of an error
        Http_response_t res;
        init_response(&res);
        req_con->status = prepare_response(&res, req_con);

        //Wait until it is this response turn
        if (mtx_lock(&rq->mtx) == thrd_error) {
            LOG(ERROR, "mtx_lock failed!");
            exit(1);
        }
        while (curr_index != rq->front) {
            cnd_wait(&rb->cnd_is_front, &rq->mtx);
        }

        //Send response
        if (req_con->status != PARSING_BROKEN_CLOSE_CONNECTION) {
            send_response(&res, tcp_con);
            DEBUG(echo_request_response_pair(req_con, &res));
        }
        else {
            LOG(INFO, "Parsing broken, closing connection!");
        }
        mtx_unlock(&rq->mtx);

        //Request block cleanup
        free_response(&res);
        clean_request_context(req_con);

        //Advance front and set nontmpty
        if (mtx_lock(&rq->mtx) == thrd_error) {
            LOG(ERROR, "mtx_lock failed!");
            exit(1);
        }
        rq->front += 1;
        rq->front %= REQUEST_QUEUE_CAPACITY;
        rq->is_nonfull = 1;
        //Move to next queue position
        curr_index += number_of_workers;
        curr_index %= REQUEST_QUEUE_CAPACITY;
        //Wake next writer if ready and queue manager if queue was full
        cnd_signal(&rq->queue[rq->front].cnd_is_front);
        cnd_signal(&rq->cnd_is_nonfull);
        if (s_request_queue_manager_finished && !rq->queue[curr_index].request_ready) {
            goto cleanup;
        }
        mtx_unlock(&rq->mtx);
    }

    //Cleanup
cleanup:
    free(response_writer_context);
    g_workers_finished += 1;
    cnd_signal(&g_cnd_worker_finished);
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
        if (mtx_lock(&rq->mtx) == thrd_error) {
            LOG(ERROR, "mtx_lock failed!");
            exit(1);
        }
        while (!rq->is_nonfull) {
            cnd_wait(&rq->cnd_is_nonfull, &rq->mtx);
        }
        Http_request_context_t* req_con = &rq->queue[(rq->rear + 1) % REQUEST_QUEUE_CAPACITY].req_con;
        mtx_unlock(&rq->mtx);
        
        req_con->status = parse_http_request(&req_con->req, tcp_con);

        //Get rq->mtx and put request to rq
        if (mtx_lock(&rq->mtx) == thrd_error) {
            LOG(ERROR, "mtx_lock failed!");
            exit(1);
        }

        //If request is broken stop further reading
        if (req_con->status == PARSING_BROKEN_CLOSE_CONNECTION) {
            abort_reading(tcp_con);
        }

        rq->rear += 1;
        rq->rear %= REQUEST_QUEUE_CAPACITY;
        if ((rq->rear + 1) % REQUEST_QUEUE_CAPACITY == rq->front) {
            rq->is_nonfull = 0;
        }
        rq->queue[rq->rear].request_ready = 1;
        cnd_signal(&rq->queue[rq->rear].cnd_request_ready);
        mtx_unlock(&rq->mtx);
    }
    if (mtx_lock(&rq->mtx) == thrd_error) {
        LOG(ERROR, "mtx_lock failed!");
        exit(1);
    }
    s_request_queue_manager_finished = 1;
    for (int i = 0; i < REQUEST_QUEUE_CAPACITY; ++i) {
        cnd_signal(&rq->queue[i].cnd_request_ready);
    }
    mtx_unlock(&rq->mtx);
}

