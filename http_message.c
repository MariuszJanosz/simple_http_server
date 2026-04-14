#include "http_message.h"
#include "log.h"
#include "stream_reader.h"

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

void init_http_message(Http_message_t* http_msg, Message_type_t type) {
    http_msg->message_type = type;
    http_msg->start_line = NULL;
    http_msg->field_lines_count = 0;
    http_msg->field_lines_capacity = 8;
    http_msg->field_lines = malloc(http_msg->field_lines_capacity *
            sizeof(*http_msg->field_lines));
    if (!http_msg->field_lines) {
        LOG(ERROR, "malloc failed!");
        exit(1);
    }
    http_msg->message_body = NULL;
}

const char* http_method_to_string(Method_t method) {
    switch (method) {
        case HTTP_GET:
            return "GET";
        case HTTP_POST:
            return "POST";
        case HTTP_PUT:
            return "PUT";
        case HTTP_DELETE:
            return "DELETE";
        case HTTP_HEAD:
            return "HEAD";
        case HTTP_OPTIONS:
            return "OPTIONS";
        case HTTP_PATCH:
            return "PATCH";
        case HTTP_UNKNOWN_METHOD:
        default:
            return "UNKNOWN";
    }
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

char* get_line(Input_queue_t* iq) {
    int new_line_found = 0;
    char* line = input_queue_get_line(iq, &new_line_found);
    while (!new_line_found) {
        char* tmp = input_queue_get_line(iq, &new_line_found);
        int line_len = strlen(line);
        int tmp_len = strlen(tmp);
        char* tmpp = realloc(line, (line_len + tmp_len + 1) * sizeof(*line));
        if (!tmpp) {
            LOG(ERROR, "realloc failed!");
            exit(1);
        }
        line = tmpp;
        strcat(line, tmp);
    }
    return line;
}

int parse_request_line(Http_message_t* http_msg, Input_queue_t* iq) {
again:
    char* input = get_line(iq);
    //RFC9112 2.2 empty line before request-line should be ignored
    if (strcmp(input, "\r\n") == 0 || strcmp(input, "\n") == 0) {
        free(input);
        input = NULL;
        goto again;
    }
    const char* delim = " \n";
    char* method = strtok(input, delim);
    char* request_target = strtok(NULL, delim);
    char* http_version = strtok(NULL, delim);
    char* leftover = strtok(NULL, "\n");
    if (!method || !request_target || !http_version) {
        LOG(INFO, "Invalid request line, to few arguments!");
        free(input);
        return 0;
    }
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
        free(rl.request_target);
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
        free(rl.request_target);
        free(rl.http_version);
        return 0;
    }
    free(input);
    http_msg->start_line = (Request_line_t*)malloc(sizeof(rl));
    memcpy(http_msg->start_line, &rl, sizeof(rl));
    return 1;
}

//For now accept everything
int is_valid_field_name(char* name) {
    return 1;
}

int is_valid_field_value(char* value) {
    return 1;
}

int parse_field_line(Http_message_t* http_msg, Input_queue_t* iq, int *is_empty) {
    char* input = get_line(iq);
    // RFC9112 2.2 field lines starting with a white space shall be ignored
    if (isspace(input[0])) {
        if (input[0] == '\r' && input[1] == '\n') {
            *is_empty = 1;
        }
        return 1;
    }
    int input_len = strlen(input);
    if (input_len == 1) {
        *is_empty = 1;
        return 1;
    }
    char* name = strtok(input, " \n");
    char* value = strtok(NULL, "\n");
    if (!name || !value) {
        LOG(INFO, "Invalid field line, to few arguments!");
        free(input);
        return 0;
    }
    Field_line_t fl;
    fl.field_name = strdup(name);
    if (!is_valid_field_name(fl.field_name)) {
        LOG(INFO, "Invalid field name!");
        free(input);
        free(fl.field_name);
        return 0;
    }
    int l = strlen(value);
    if (l > 0 && value[l - 1] == '\r') {
        value[l - 1] = '\0';
    }
    fl.field_value = strdup(value);
    if (!is_valid_field_value(fl.field_value)) {
        LOG(INFO, "Invalid field value!");
        free(input);
        free(fl.field_name);
        free(fl.field_value);
        return 0;
    }
    free(input);
    if (http_msg->field_lines_count == http_msg->field_lines_capacity) {
        http_msg->field_lines_capacity *= 2;
        Field_line_t* tmp = realloc(http_msg->field_lines, http_msg->field_lines_capacity *
                sizeof(*http_msg->field_lines));
        if (!tmp) {
            LOG(ERROR, "realloc failed!");
            exit(1);
        }
        http_msg->field_lines = tmp;
    }
    memcpy(&http_msg->field_lines[http_msg->field_lines_count], &fl, sizeof(fl));
    http_msg->field_lines_count += 1;
    return 1;
}

//For now requests cannot have bodies
int read_body(Http_message_t* http_msg, Input_queue_t* iq) {
    return 0;
}

int should_have_body(Http_message_t* http_msg) {
    return 0;
}

int parse_http_request(Http_message_t* http_msg, Input_queue_t* iq) {
    int res = 1;
    res = res && parse_request_line(http_msg, iq);
    if (!res) {
        LOG(INFO, "parse_request_line failed!");
        return res;
    }
    int is_empty = 0;
    while (!is_empty) {
        res = res && parse_field_line(http_msg, iq, &is_empty);
        if (!res) {
            LOG(INFO, "parse_field_line failed!");
            return res;
        }
    }
    if (should_have_body(http_msg)) {
        res = res && read_body(http_msg, iq);
        if (!res) {
            LOG(INFO, "read_body failed!");
            return res;
        }
    }
    return res;
}

