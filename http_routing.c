#include "http_routing.h"
#include "http_request_context.h"
#include "http_response.h"

#include <string.h>
#include <limits.h>

Http_status_t route_http_request(   Http_request_context_t* req_con,
                                    Http_response_t* res,
                                    char** resource_rel_path) {
    StringView req_tar = req_con->uri.path;
    size_t len = strlen(g_www_root);
    *resource_rel_path = NULL;
    Http_status_t status = ROUTING_FINE;

    if (len + req_tar.len + 1 > PATH_MAX) {
        return HTTP_STATUS_URI_TOO_LONG;
    }
    else if (    strncmp(req_tar.cstr, "/", req_tar.len) == 0 ||
            strncmp(req_tar.cstr, "/index.html", req_tar.len) == 0) {
        *resource_rel_path = "/index.html";
    }
    else if (   strncmp(req_tar.cstr, "/chunked", req_tar.len) == 0 ||
                strncmp(req_tar.cstr, "/chunked.html", req_tar.len) == 0) {
        *resource_rel_path = "/chunked.html";
    }
    else if (strncmp(req_tar.cstr, "/nggyu", req_tar.len) == 0) {
        *resource_rel_path = "/nggyu.html";
    }
    else if (strncmp(req_tar.cstr, "/nggyu.mp4", req_tar.len) == 0) {
        *resource_rel_path = "/nggyu.mp4";
    }
    else {
        *resource_rel_path = "/404.html";
        status = HTTP_STATUS_NOT_FOUND;
    }

    strcpy(res->resource_path, g_www_root);
    strcpy(res->resource_path + len, *resource_rel_path);
    return status;
}

