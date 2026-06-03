#include "http_routing.h"
#include "http_request_context.h"
#include "http_response.h"

#include <string.h>
#include <limits.h>

Http_status_t route_http_request(   Http_request_context_t* req_con,
                                    Http_response_t* res) {
    StringView req_tar = req_con->uri.path;
    size_t len = strlen(g_www_root);
    char* target = NULL;
    Http_status_t status = ROUTING_FINE;

    if (len + req_tar.len + 1 > PATH_MAX) {
        return HTTP_STATUS_URI_TOO_LONG;
    }
    else if (    strncmp(req_tar.cstr, "/", strlen("/")) == 0 ||
            strncmp(req_tar.cstr, "/index.html", strlen("/index.html")) == 0) {
        target = "/index.html";
    }
    else if (   strncmp(req_tar.cstr, "/chunked", strlen("/chunked")) == 0 ||
                strncmp(req_tar.cstr, "/chunked.html", strlen("/chunked.html")) == 0) {
        target = "/chunked.html";
    }
    else if (strncmp(req_tar.cstr, "/nggyu", strlen("/nggyu")) == 0) {
        target = "/nggyu.html";
    }
    else if (strncmp(req_tar.cstr, "/nggyu.mp4", strlen("/nggyu.mp4")) == 0) {
        target = "/nggyu.mp4";
    }
    else {
        target = "/404.html";
        status = HTTP_STATUS_NOT_FOUND;
    }

    strcpy(res->resource_path, g_www_root);
    strcpy(res->resource_path + len, target);
    return status;
}

