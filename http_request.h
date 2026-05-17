#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include "http_field_line.h"
#include "tcp_connection.h"
#include "status.h"

#include <stddef.h>

typedef struct Http_request_line_t {
    const char* method;
    char* target;
    const char* version;
} Http_request_line_t;

typedef struct Http_request_t {
    Http_request_line_t request_line;
    Field_line_hash_map_t headers;
    char* body;
    size_t body_size;
    Field_line_hash_map_t trailers;
} Http_request_t;

void init_http_request(Http_request_t* req);
void free_http_request(Http_request_t* req);
Http_status_t parse_http_request(Http_request_t* req, Tcp_connection_t tcp_con);

#endif //HTTP_REQUEST_H

