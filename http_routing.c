#include "http_routing.h"
#include "log.h"

#include <string.h>
#include <limits.h>

#include <stdlib.h>

void route_http_request(Http_request_context_t* req_con, Http_response_t* res) {
    char* path = strndup(req_con->uri.path.cstr, req_con->uri.path.len);
    if (!path) {
        LOG(ERROR, "strndup failed!");
        exit(1);
    }
    ssize_t ind = resource_index_for_path(path);
    free(path);
    res->resource_index = ind;
}

