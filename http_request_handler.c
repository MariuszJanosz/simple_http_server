#include "http_request_handler.h"
#include "http_routing.h"
#include "log.h"

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

#include <fcntl.h>

#define CHUNKING_THRESHOLD 1024

extern char g_www_root[PATH_MAX];

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

Response_method_t http_get_handler(Http_message_t* req, Http_message_t* res, Http_status_t* status) {
    char* body = NULL;
    size_t body_len = 0;
    switch (*status) {
        case HTTP_STATUS_OK:
            {
                char route[PATH_MAX];
                *status = route_http_request(req, route);
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
                if (fs > CHUNKING_THRESHOLD) {
                    fclose(f);
                    res->body_fd = open(route, O_RDONLY);
                    if (res->body_fd == -1) {
                        LOG(ERROR, "open failed!");
                        exit(1);
                    }
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
    write_response_status_line(res, "HTTP/1.1",
            (const char*)http_status_to_string_num(*status),
            (const char*)http_status_to_string(*status));
    if (body_len) {
        char size[1024];
        sprintf(size, "%zu", body_len);
        write_response_field_line(res, "Content-length", size);
        write_response_field_line(res, "Content-Type", "text/html");
        write_response_field_line(res, "Connection", "close");
        write_response_body_content_length(res, body, body_len);
        return RESPONSE_CONTENT_LENGTH;
    }
    else if (res->body_fd != -1) {
        write_response_field_line(res, "Transfer-Encoding", "chunked");
        write_response_field_line(res, "Content-Type", "video/mp4");
        return RESPONSE_TRANSFER_ENCODING_CHUNKED;
    }
    return RESPONSE_NO_BODY;
}

//TODO
Response_method_t http_post_handler(Http_message_t* req, Http_message_t* res, Http_status_t* status) {
    *status = HTTP_STATUS_NOT_IMPLEMENTED;
    write_response_status_line(res, "HTTP/1.1",
            (const char*)http_status_to_string_num(*status),
            (const char*)http_status_to_string(*status));
    return RESPONSE_NO_BODY;
}

Response_method_t http_put_handler(Http_message_t* req, Http_message_t* res, Http_status_t* status) {
    *status = HTTP_STATUS_NOT_IMPLEMENTED;
    write_response_status_line(res, "HTTP/1.1",
            (const char*)http_status_to_string_num(*status),
            (const char*)http_status_to_string(*status));
    return RESPONSE_NO_BODY;
}

Response_method_t http_delete_handler(Http_message_t* req, Http_message_t* res, Http_status_t* status) {
    *status = HTTP_STATUS_NOT_IMPLEMENTED;
    write_response_status_line(res, "HTTP/1.1",
            (const char*)http_status_to_string_num(*status),
            (const char*)http_status_to_string(*status));
    return RESPONSE_NO_BODY;
}

Response_method_t http_head_handler(Http_message_t* req, Http_message_t* res, Http_status_t* status) {
    *status = HTTP_STATUS_NOT_IMPLEMENTED;
    write_response_status_line(res, "HTTP/1.1",
            (const char*)http_status_to_string_num(*status),
            (const char*)http_status_to_string(*status));
    return RESPONSE_NO_BODY;
}

Response_method_t http_options_handler(Http_message_t* req, Http_message_t* res, Http_status_t* status) {
    *status = HTTP_STATUS_NOT_IMPLEMENTED;
    write_response_status_line(res, "HTTP/1.1",
            (const char*)http_status_to_string_num(*status),
            (const char*)http_status_to_string(*status));
    return RESPONSE_NO_BODY;
}

Response_method_t http_patch_handler(Http_message_t* req, Http_message_t* res, Http_status_t* status) {
    *status = HTTP_STATUS_NOT_IMPLEMENTED;
    write_response_status_line(res, "HTTP/1.1",
            (const char*)http_status_to_string_num(*status),
            (const char*)http_status_to_string(*status));
    return RESPONSE_NO_BODY;
}

Response_method_t http_unknown_method_handler(Http_message_t* req, Http_message_t* res, Http_status_t* status) {
    *status = HTTP_STATUS_NOT_IMPLEMENTED;
    write_response_status_line(res, "HTTP/1.1",
            (const char*)http_status_to_string_num(*status),
            (const char*)http_status_to_string(*status));
    return RESPONSE_NO_BODY;
}

Response_method_t handle_http_request(Http_message_t* req, Http_message_t* res, Http_status_t* status) {
    Method_t method = ((Request_line_t*)req->start_line)->method;
    switch (method) {
        case HTTP_GET:
            return http_get_handler(req, res, status);
            break;
        case HTTP_POST:
            return http_post_handler(req, res, status);
            break;
        case HTTP_PUT:
            return http_put_handler(req, res, status);
            break;
        case HTTP_DELETE:
            return http_delete_handler(req, res, status);
            break;
        case HTTP_HEAD:
            return http_head_handler(req, res, status);
            break;
        case HTTP_OPTIONS:
            return http_options_handler(req, res, status);
            break;
        case HTTP_PATCH:
            return http_patch_handler(req, res, status);
            break;
        case HTTP_UNKNOWN_METHOD:
        default:
            return http_unknown_method_handler(req, res, status);
            break;
    }
}

