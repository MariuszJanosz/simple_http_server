#include "http_routing.h"
#include "log.h"

#include <string.h>
#include <limits.h>

#include <stdlib.h>

void route_http_request(Http_request_context_t* req_con, Http_response_t* res) {
    char path[PATH_MAX];
    if (req_con->uri.path.len + 1 > PATH_MAX) {
        LOG(ERROR, "path too long!");
        exit(1);
    }
    strncpy(path, req_con->uri.path.cstr, req_con->uri.path.len);
    path[req_con->uri.path.len] = '\0';
    ssize_t ind = resource_index_for_path(path);
    res->resource_index = ind;
}

