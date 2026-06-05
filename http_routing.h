#ifndef HTTP_ROUTING_H
#define HTTP_ROUTING_H

#include "http_request_context.h"
#include "http_response.h"

#include <limits.h>

extern char g_www_root[PATH_MAX];

Http_status_t route_http_request(   Http_request_context_t* req_con,
                                    Http_response_t* res,
                                    char** resource_rel_path);

#endif //HTTP_ROUTING_H

