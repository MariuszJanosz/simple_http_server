#ifndef HTTP_ROUTING_H
#define HTTP_ROUTING_H

#include "http_request_context.h"
#include "http_response.h"
#include "http_resources.h"

#include <limits.h>

extern char g_www_root[PATH_MAX];

void route_http_request(Http_request_context_t* req_con, Http_response_t* res);

#endif //HTTP_ROUTING_H

