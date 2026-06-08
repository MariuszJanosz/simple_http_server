#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include "http_request_context.h"
#include "status.h"
#include "tcp_connection.h"
#include "http_field_line.h"

#include <limits.h>

#include <sys/types.h>

typedef enum Http_body_section_type_t {
    FILE_DESCRIPTOR,
    CHAR_BUFFER
} Http_body_section_type_t;

typedef struct Http_body_section_FILE_DESCRIPTOR_t {
    int fd;
    size_t size;
} Http_body_section_FILE_DESCRIPTOR_t;

typedef struct Http_body_section_CHAR_BUFFER_t {
    char* buffer;
    size_t size;
} Http_body_section_CHAR_BUFFER_t;

typedef union Http_any_body_section_type_t {
    Http_body_section_FILE_DESCRIPTOR_t fd_section;
    Http_body_section_CHAR_BUFFER_t char_buff_section;
} Http_any_body_section_type_t;

typedef struct Http_response_body_t {
    Http_any_body_section_type_t* sections;
    Http_body_section_type_t* section_types;
    size_t count;
    size_t capacity;
    size_t size;
} Http_response_body_t;

typedef struct Http_response_t {
    const char* status_line;
    Field_line_hash_map_t headers_hm;
    char* headers;
    ssize_t resource_index;
    Http_response_body_t body;
    Field_line_hash_map_t trailers_hm;
    char* trailers;
    int has_body;
    int has_headers_hm;
    int has_trailers_hm;
    int send_chunked;
    int should_close;
} Http_response_t;

void init_response(Http_response_t* res);
void free_response(Http_response_t* res);
Http_status_t prepare_response(Http_response_t* res, Http_request_context_t* req_con);
void send_response(Http_response_t* res, Tcp_connection_t tcp_con);
void print_response(Http_response_t* res);

#endif //HTTP_RESPONSE_H

