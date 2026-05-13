#ifndef HTTP_REQUEST_HANDLER_H
#define HTTP_REQUEST_HANDLER_H

#include "http_message.h"

typedef enum Response_method_t {
    RESPONSE_NO_BODY,
    RESPONSE_CONTENT_LENGTH,
    RESPONSE_TRANSFER_ENCODING_CHUNKED,
    RESPONSE_ABORT
} Response_method_t;

Response_method_t handle_http_request(Http_message_t* req, Http_message_t* res, Http_status_t* status);

#endif //HTTP_REQUEST_HANDLER_H

