#ifndef HTTP_REQUEST_HANDLER_H
#define HTTP_REQUEST_HANDLER_H

#include "http_message.h"

void handle_http_request(Http_message_t* req, Http_message_t* res, Http_status_t* status);

#endif //HTTP_REQUEST_HANDLER_H

