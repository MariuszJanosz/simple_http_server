#include "log.h"
#include "http_request_queue.h"
#include "http_routing.h"
#include "tcp_connection.h"
#include "http_message.h"

#include <threads.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

void echo_request(Http_message_t* req) {
    static int req_id = 0;
    Request_line_t* req_line = (Request_line_t*)req->start_line;
    printf("---req---echo---id:%05d---\n", req_id++);
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
}

long get_file_size(FILE* f) {
    long curr = ftell(f);
    if (curr < 0) {
        LOG(ERROR, "ftell failed!");
    }
    if (fseek(f, 0, SEEK_END)) {
        LOG(ERROR, "fseek failed!");
        exit(1);
    }
    long res = ftell(f);
    if (res < 0) {
        LOG(ERROR, "ftell failed!");
        exit(1);
    }
    if (fseek(f, curr, SEEK_SET)) {
        LOG(ERROR, "fseek failed!");
        exit(1);
    }
    return res;
}

void init_request_queue(Request_queue_t* rq) {
    for (int i = 0; i < REQUEST_QUEUE_CAPACITY; ++i) {
        Request_response_pair_t* rrp = &rq->queue[i];
        rrp->request_ready = 0;
        if (cnd_init(&rrp->cnd_request_ready) != thrd_success) {
            LOG(ERROR, "cnd_init failed!");
            exit(1);
        }
        if (cnd_init(&rrp->cnd_is_front) != thrd_success) {
            LOG(ERROR, "cnd_init failed!");
            exit(1);
        }
    }
    rq->front = 0;
    rq->rear = 0;
    rq->is_nonfull = 1;
    rq->is_empty = 1;

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
        Request_response_pair_t* rrp = &rq->queue[i];
        cnd_destroy(&rrp->cnd_request_ready);
        cnd_destroy(&rrp->cnd_is_front);
    }
    mtx_destroy(&rq->mtx);
    cnd_destroy(&rq->cnd_is_nonfull);
}

int response_writer_thr(void* response_writer_context) {
    Request_queue_t* rq = ((Response_writer_context_t*)(response_writer_context))->rq;
    int number_of_workers = ((Response_writer_context_t*)(response_writer_context))->number_of_workers;
    int curr_index = ((Response_writer_context_t*)(response_writer_context))->start_index;
    Tcp_connection_t tcp_con = ((Response_writer_context_t*)(response_writer_context))->tcp_con;
    char* www_root = ((Response_writer_context_t*)(response_writer_context))->www_root;
    
    while (1) {
        //Wait until request is ready
        if (mtx_lock(&rq->mtx) == thrd_error) {
            LOG(ERROR, "mtx_lock failed!");
            exit(1);
        }
        Request_response_pair_t* rrp = &rq->queue[curr_index];
        while (!rrp->request_ready) {
            cnd_wait(&rrp->cnd_request_ready, &rq->mtx);
        }
        mtx_unlock(&rq->mtx);
        rrp->request_ready = 0;

        //Prepare response
        Http_message_t *req = rrp->req;
        Http_status_t status = rrp->status;
        Http_message_t res;
        rrp->res = &res;
        init_http_message(&res, HTTP_RESPONSE);
        char* body = NULL;
        char size[1024];
        size_t body_len = 0;
        switch (status) {
            case HTTP_STATUS_OK:
                {
                    char route[PATH_MAX];
                    status = route_http_request(req, route, www_root);
                    FILE* f = fopen(route, "rb");
                    if (!f) {
                        LOG(ERROR, "fopen failed!");
                        exit(1);
                    }
                    long fs = get_file_size(f);
                    if (fs == 0) {
                        fclose(f);
                        break;
                    }
                    body = malloc(fs);
                    if (!body) {
                        LOG(ERROR, "malloc failed!");
                        exit(1);
                    }
                    if (fread(body, 1, fs, f) < fs) {
                        LOG(ERROR, "fread failed!");
                        exit(1);
                    }
                    fclose(f);
                    body_len = fs;
                }
                break;
            default:
                {
                    
                }
                break;
        }
        char status_char[4];
        sprintf(status_char, "%d", status);
        write_response_status_line(&res, "HTTP/1.1",
                status_char, (char*)http_status_to_string(status));
        if (body_len) {
            sprintf(size, "%zu", body_len);
            write_response_field_line(&res, "Content-length", size);
            write_response_body_content_length(&res, body, body_len);
        }

        //Wait until it is this response turn
        if (mtx_lock(&rq->mtx) == thrd_error) {
            LOG(ERROR, "mtx_lock failed!");
            exit(1);
        }
        while (curr_index != rq->front) {
            cnd_wait(&rrp->cnd_is_front, &rq->mtx);
        }
        mtx_unlock(&rq->mtx);
        
        //Send response
        send_response(tcp_con, &res);

        echo_response(&res);

        //Cleanup
        if (mtx_lock(&rq->mtx) == thrd_error) {
            LOG(ERROR, "mtx_lock failed!");
            exit(1);
        }
        free_http_message(rrp->req);
        free(rrp->req);
        free_http_message(rrp->res);
        if (rq->front == rq->rear) {
            rq->is_empty = 1;
            rq->rear += 1;
            rq->rear %= REQUEST_QUEUE_CAPACITY;
        }
        rq->front += 1;
        rq->front %= REQUEST_QUEUE_CAPACITY;
        rq->is_nonfull = 1;
        cnd_signal(&rq->cnd_is_nonfull);
        curr_index += number_of_workers;
        curr_index %= REQUEST_QUEUE_CAPACITY;
        cnd_signal(&rq->queue[curr_index].cnd_is_front);
        mtx_unlock(&rq->mtx);
    }
    
    free(response_writer_context);
    thrd_exit(0);
}

void init_writers(Request_queue_t* rq, int number_of_workers, Tcp_connection_t tcp_con, char* www_root) {
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
        rwc->www_root = www_root;

        thrd_t thr;
        thrd_create(&thr, response_writer_thr, rwc);
        thrd_detach(thr);
    }
}


void request_queue_manager(Request_queue_t* rq, Input_queue_t* iq) {
    while (1) {
        //prepare next request
        Http_message_t* req = malloc(sizeof(*req));
        if (!req) {
            LOG(ERROR, "malloc failed!");
            exit(1);
        }
        init_http_message(req, HTTP_REQUEST);
        Http_status_t status = parse_http_request(req, iq);
        echo_request(req);

        //Get rq->mtx and put request to rq
        if (mtx_lock(&rq->mtx) == thrd_error) {
            LOG(ERROR, "mtx_lock failed!");
            exit(1);
        }
        while (!rq->is_nonfull) {
            cnd_wait(&rq->cnd_is_nonfull, &rq->mtx);
        }
        if (!rq->is_empty) {
            rq->rear += 1;
            rq->rear %= REQUEST_QUEUE_CAPACITY;
        }
        if ((rq->rear + 1) % REQUEST_QUEUE_CAPACITY == rq->front) {
            rq->is_nonfull = 0;
        }
        Request_response_pair_t* rrp = &rq->queue[rq->rear];
        rrp->req = req;
        rrp->status = status;
        rrp->request_ready = 1;
        cnd_signal(&rrp->cnd_request_ready);
        mtx_unlock(&rq->mtx);
    }
}

