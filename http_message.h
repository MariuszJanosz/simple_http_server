#ifndef HTTP_MESSAGE_H
#define HTTP_MESSAGE_H

#include "stream_reader.h"

typedef enum {
    HTTP_REQUEST,
    HTTP_RESPONSE
} Message_type_t;

typedef enum {
    HTTP_GET,
    HTTP_POST,
    HTTP_PUT,
    HTTP_DELETE,
    HTTP_HEAD,
    HTTP_OPTIONS,
    HTTP_PATCH,
    HTTP_UNKNOWN_METHOD
} Method_t;

typedef struct {
    Method_t method;
    char* request_target;
    char* http_version;
} Request_line_t;

typedef struct {
    char* http_version;
    char* status_code;
    char* status_text;
} Status_line_t;

typedef struct {
    char* field_name;
    char* field_value;
} Field_line_t;

typedef struct Http_message_t {
    Message_type_t message_type;
    void* start_line; //request=(Request_line_t*), response=(Status_line_t*)
    Field_line_t* field_lines;
    int field_lines_count;
    int field_lines_capacity;
    char* message_body;
} Http_message_t;

void init_http_message(Http_message_t* http_msg, Message_type_t type);
int parse_request_line(Http_message_t* http_msg, Input_queue_t* iq);
int parse_field_line(Http_message_t* http_msg, Input_queue_t* iq, int *is_empty);
int read_body(Http_message_t* http_msg, Input_queue_t* iq);
int parse_http_request(Http_message_t* http_msg, Input_queue_t* iq);

const char* http_method_to_string(Method_t method);

#endif //HTTP_MESSAGE_H

