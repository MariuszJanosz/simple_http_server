#include "http_request_handler.h"
#include "http_routing.h"
#include "log.h"

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

extern char www_root[PATH_MAX];

long get_file_size(FILE* f) {
    long curr = ftell(f);
    if (curr < 0) {
        LOG(ERROR, "ftell failed!");
    }
    if (fseek(f, 0, SEEK_END)) {
        LOG(ERROR, "fseek failed!");
        exit(1);
    }
    long res = ftell(f);
    if (res < 0) {
        LOG(ERROR, "ftell failed!");
        exit(1);
    }
    if (fseek(f, curr, SEEK_SET)) {
        LOG(ERROR, "fseek failed!");
        exit(1);
    }
    return res;
}

void http_get_handler(Http_message_t* req, Http_message_t* res, Http_status_t* status) {
    char* body = NULL;
    char size[1024];
    size_t body_len = 0;
    switch (*status) {
        case HTTP_STATUS_OK:
            {
                char route[PATH_MAX];
                *status = route_http_request(req, route, www_root);
                if (*status != HTTP_STATUS_OK && *status != HTTP_STATUS_NOT_FOUND) {
                    break;
                }
                FILE* f = fopen(route, "rb");
                if (!f) {
                    LOG(ERROR, "fopen failed!");
                    exit(1);
                }
                long fs = get_file_size(f);
                if (fs == 0) {
                    fclose(f);
                    break;
                }
                body = malloc(fs);
                if (!body) {
                    LOG(ERROR, "malloc failed!");
                    exit(1);
                }
                if (fread(body, 1, fs, f) < fs) {
                    LOG(ERROR, "fread failed!");
                    exit(1);
                }
                fclose(f);
                body_len = fs;
            }
            break;
        default:
            {
                 
            }
            break;
    }
    char status_char[4];
    sprintf(status_char, "%d", *status);
    write_response_status_line(res, "HTTP/1.1",
            status_char, (char*)http_status_to_string(*status));
    if (body_len) {
        sprintf(size, "%zu", body_len);
        write_response_field_line(res, "Content-length", size);
        write_response_body_content_length(res, body, body_len);
    }
}

//TODO
void http_post_handler(Http_message_t* req, Http_message_t* res, Http_status_t* status) {

}

void http_put_handler(Http_message_t* req, Http_message_t* res, Http_status_t* status) {

}

void http_delete_handler(Http_message_t* req, Http_message_t* res, Http_status_t* status) {

}

void http_head_handler(Http_message_t* req, Http_message_t* res, Http_status_t* status) {

}

void http_options_handler(Http_message_t* req, Http_message_t* res, Http_status_t* status) {

}

void http_patch_handler(Http_message_t* req, Http_message_t* res, Http_status_t* status) {

}

void http_unknown_method_handler(Http_message_t* req, Http_message_t* res, Http_status_t* status) {

}

void handle_http_request(Http_message_t* req, Http_message_t* res, Http_status_t* status) {
    Method_t method = ((Request_line_t*)req->start_line)->method;
    switch (method) {
        case HTTP_GET:
            http_get_handler(req, res, status);
            break;
        case HTTP_POST:
            http_post_handler(req, res, status);
            break;
        case HTTP_PUT:
            http_put_handler(req, res, status);
            break;
        case HTTP_DELETE:
            http_delete_handler(req, res, status);
            break;
        case HTTP_HEAD:
            http_head_handler(req, res, status);
            break;
        case HTTP_OPTIONS:
            http_options_handler(req, res, status);
            break;
        case HTTP_PATCH:
            http_patch_handler(req, res, status);
            break;
        case HTTP_UNKNOWN_METHOD:
        default:
            http_unknown_method_handler(req, res, status);
            break;
    }
}

