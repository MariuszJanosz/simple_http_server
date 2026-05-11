#ifndef HTTP_ROUTING_H
#define HTTP_ROUTING_H

#include "http_message.h"

#include <limits.h>

extern char g_www_root[PATH_MAX];

Http_status_t route_http_request(Http_message_t* req, char* route);

#endif //HTTP_ROUTING_H

