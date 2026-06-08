#ifndef HTTP_REQUEST_CONTEXT_H
#define HTTP_REQUEST_CONTEXT_H

#include "http_request.h"
#include "status.h"
#include "uri_parser.h"

typedef struct Http_request_context_t {
    Http_request_t req;
    Http_status_t status;
    //Additional fields describing processed request would go here
    URI uri;
    int close_connection_after_response;
} Http_request_context_t;

void init_request_context(Http_request_context_t* req_con);
void free_request_context(Http_request_context_t* req_con);
void clean_request_context(Http_request_context_t* req_con);
Http_status_t process_request(Http_request_context_t* req_con);
void print_request_context(Http_request_context_t* req_con);

#endif //HTTP_REQUEST_CONTEXT_H

