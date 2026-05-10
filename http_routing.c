#include "http_routing.h"
#include "http_message.h"

#include <string.h>
#include <limits.h>

Http_status_t route_http_request(Http_message_t* req, char* route, char* www_root) {
    char* req_tar = ((Request_line_t*)(req->start_line))->request_target;
    size_t len = strlen(www_root);
    char* target = NULL;
    Http_status_t res = HTTP_STATUS_OK;

    if (len + strlen(req_tar) + 1 > PATH_MAX) {
        return HTTP_STATUS_URI_TOO_LONG;
    }
    else if (    strcmp(req_tar, "/") == 0 ||
            strcmp(req_tar, "/index.html") == 0) {
        target = "/index.html";
    }
    else {
        target = "/404.html";
        res = HTTP_STATUS_NOT_FOUND;
    }

    strcpy(route, www_root);
    strcpy(route + len, target);
    return res;
}

