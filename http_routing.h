#ifndef HTTP_ROUTING_H
#define HTTP_ROUTING_H

#include "http_message.h"

Http_status_t route_http_request(Http_message_t* req, char* route, char* www_root);

#endif //HTTP_ROUTING_H

