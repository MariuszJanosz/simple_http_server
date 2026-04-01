#include "http_message.h"
#include "log.h"

#include <string.h>
#include <stdlib.h>

void init_http_message(Http_message_t* http_msg, Message_type_t type) {
    http_msg->message_type = type;
    http_msg->start_line = NULL;
    http_msg->field_lines = NULL;
    http_msg->field_lines_count = 0;
    http_msg->message_body = NULL;
}

Method_t str_to_http_method(char* str) {
    //lazy implementation
    if (strcmp(str, "GET") == 0) {
        return HTTP_GET;
    }
    else if (strcmp(str, "POST") == 0) {
        return HTTP_POST;
    }
    else if (strcmp(str, "PUT") == 0) {
        return HTTP_PUT;
    }
    else if (strcmp(str, "DELETE") == 0) {
        return HTTP_DELETE;
    }
    else if (strcmp(str, "HEAD") == 0) {
        return HTTP_HEAD;
    }
    else if (strcmp(str, "OPTIONS") == 0) {
        return HTTP_OPTIONS;
    }
    else if (strcmp(str, "PATCH") == 0) {
        return HTTP_PATCH;
    }
    else {
        return HTTP_UNKNOWN_METHOD;
    }
}

int is_valid_request_target(char* str) {
    //TODO for now assume it is a valid request target
    return 1;
}

int is_valid_http_version(char* str) {
    //support http/1.1 only
    if (strcmp(str, "HTTP/1.1") == 0) {
        return 1;
    }
    return 0;
}

int parse_request_line(Http_message_t* http_msg, char* input) {
    const char* delim = " \n";
    char* method = strtok(input, delim);
    char* request_target = strtok(NULL, delim);
    char* http_version = strtok(NULL, delim);
    char* leftover = strtok(NULL, delim);
    if (leftover) {
        LOG(INFO, "Invalid request line, to many arguments!");
        free(input);
        return 0;
    }
    Request_line_t rl;
    rl.method = str_to_http_method(method);
    if (rl.method == HTTP_UNKNOWN_METHOD) {
        LOG(INFO, "Unknown http method!");
        free(input);
        return 0;
    }
    rl.request_target = strdup(request_target);
    if (!is_valid_request_target(rl.request_target)) {
        LOG(INFO, "Invalid http request target!");
        free(input);
        return 0;
    }
    int l = strlen(http_version);
    if (l > 0 && http_version[l - 1] == '\r') {
        http_version[l - 1] = '\0';
    }
    rl.http_version = strdup(http_version);
    if (!is_valid_http_version(rl.http_version)) {
        LOG(INFO, "Invalid http version!");
        free(input);
        return 0;
    }
    free(input);
    http_msg->start_line = (Request_line_t*)malloc(sizeof(rl));
    memcpy(http_msg->start_line, &rl, sizeof(rl));
    return 1;
}

