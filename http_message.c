#include "http_message.h"
#include "log.h"
#include "stream_reader.h"
#include "uri.h"

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

int is_valid_absolute_form(const char* str) {
    return uri_is_absolute_uri(str, strlen(str));
}

int is_valid_origin_form(const char* str) {
    /*origin-form = 1*( "/" segment ) [ "?" query ]*/
    int i = 0;
    int len;
    len = strlen(str);
    while (i < len) {
        if (0x3F == str[i] /*?*/) {
            break;
        }
        i += 1;
    }
    if (i == len) { /*there is no ( "?" query ) part*/
        return (len > 0) && uri_is_path_abempty(str, len);
    }
    return  (i > 0) &&
            uri_is_path_abempty(str, i) &&
            uri_is_query(str + i + 1, len - (i + 1));
}

int is_valid_authority_form(const char* str) {
    int i = 0;
    int len = 0;
    len = strlen(str);
    while (i < len) {
        if (0x3A == str[i]) {
            break;
        }
        i += 1;
    }
    if (i == len) {
        return 0;
    }
    return  uri_is_host(str, i) &&
            uri_is_port(str + i + 1, len - (i + 1));
}

Http_status_t is_valid_request_target(char* str) {
    char* ptr = str;
    while (*ptr) {
        if (isspace(*ptr)) { //No spaces allowed RFC9112 3.2
            return HTTP_STATUS_BAD_REQUEST;
        }
        ptr += 1;
    }
    if (str[0] == '*') { //Asterisk-form
        if (str[1] != '\0') {
            return HTTP_STATUS_BAD_REQUEST;
        }
    }
    else {
        if (!is_valid_absolute_form(str) &&
            !is_valid_origin_form(str) &&
            !is_valid_authority_form(str)) {
            return HTTP_STATUS_BAD_REQUEST;
        }
    }
    return HTTP_STATUS_OK;
}

Http_status_t is_valid_http_version(char* str) {
    //support http/1.1 only
    if (strcmp(str, "HTTP/1.1") == 0) {
        return HTTP_STATUS_OK;
    }
    return HTTP_STATUS_HTTP_VERSION_NOT_SUPPORTED;
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

Http_status_t parse_request_line(Http_message_t* http_msg, Input_queue_t* iq) {
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
        return HTTP_STATUS_BAD_REQUEST;
    }
    if (leftover) {
        LOG(INFO, "Invalid request line, to many arguments!");
        free(input);
        return HTTP_STATUS_BAD_REQUEST;
    }
    Request_line_t rl;
    rl.method = str_to_http_method(method);
    if (rl.method == HTTP_UNKNOWN_METHOD) {
        LOG(INFO, "Unknown http method!");
        free(input);
        return HTTP_STATUS_BAD_REQUEST;
    }
    rl.request_target = strdup(request_target);
    if (is_valid_request_target(rl.request_target) != HTTP_STATUS_OK) {
        LOG(INFO, "Invalid http request target!");
        free(input);
        free(rl.request_target);
        return HTTP_STATUS_BAD_REQUEST;
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
        return HTTP_STATUS_HTTP_VERSION_NOT_SUPPORTED;
    }
    free(input);
    http_msg->start_line = (Request_line_t*)malloc(sizeof(rl));
    memcpy(http_msg->start_line, &rl, sizeof(rl));
    return HTTP_STATUS_OK;
}

//For now accept everything
Http_status_t is_valid_field_name(char* name) {
    return HTTP_STATUS_OK;
}

Http_status_t is_valid_field_value(char* value) {
    return HTTP_STATUS_OK;
}

Http_status_t parse_field_line(Http_message_t* http_msg, Input_queue_t* iq, int *is_empty) {
    char* input = get_line(iq);
    // RFC9112 2.2 field lines starting with a white space shall be ignored
    if (isspace(input[0])) {
        if ((input[0] == '\r' && input[1] == '\n') || input[0] == '\n') {
            *is_empty = 1;
        }
        return HTTP_STATUS_OK;
    }
    char* name = strtok(input, " \n");
    char* value = strtok(NULL, "\n");
    if (!name || !value) {
        LOG(INFO, "Invalid field line, to few arguments!");
        free(input);
        return HTTP_STATUS_BAD_REQUEST;
    }
    Field_line_t fl;
    fl.field_name = strdup(name);
    if (!is_valid_field_name(fl.field_name)) {
        LOG(INFO, "Invalid field name!");
        free(input);
        free(fl.field_name);
        return HTTP_STATUS_BAD_REQUEST;
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
        return HTTP_STATUS_BAD_REQUEST;
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
    return HTTP_STATUS_OK;
}

//For now requests cannot have bodies
Http_status_t read_body(Http_message_t* http_msg, Input_queue_t* iq) {
    return 0;
}

int should_have_body(Http_message_t* http_msg) {
    return 0;
}

Http_status_t parse_http_request(Http_message_t* http_msg, Input_queue_t* iq) {
    Http_status_t status = parse_request_line(http_msg, iq);
    int is_empty = 0;
    while (!is_empty) {
        if (status == HTTP_STATUS_OK) {
            status = parse_field_line(http_msg, iq, &is_empty);
        }
        else {
            parse_field_line(http_msg, iq, &is_empty);
        }
    }
    if (should_have_body(http_msg)) {
        if (status == HTTP_STATUS_OK) {
            status = read_body(http_msg, iq);
        }
        else {
            read_body(http_msg, iq);
        }
    }
    return status;
}

const char* http_status_to_string(Http_status_t status) {
    switch (status) {
        case HTTP_STATUS_CONTINUE:
            return "Continue";
        case HTTP_STATUS_SWITCHING_PROTOCOLS:
            return "Switching protocols";
        case HTTP_STATUS_PROCESSING:
            return "Processing";
        case HTTP_STATUS_EARLY_HINTS:
            return "Early hints";
        case HTTP_STATUS_OK:
            return "OK";
        case HTTP_STATUS_CREATED:
            return "Created";
        case HTTP_STATUS_ACCEPTED:
            return "Accepted";
        case HTTP_STATUS_NON_AUTHORITATIVE_INFORMATION:
            return "Non-authoritative information";
        case HTTP_STATUS_NO_CONTENT:
            return "No content";
        case HTTP_STATUS_RESET_CONTENT:
            return "Reset content";
        case HTTP_STATUS_PARTIAL_CONTENT:
            return "Partial content";
        case HTTP_STATUS_MULTI_STATUS:
            return "Multi status";
        case HTTP_STATUS_ALREADY_REPORTED:
            return "Already reported";
        case HTTP_STATUS_IM_USED:
            return "IM used";
        case HTTP_STATUS_MULTIPLE_CHOICES:
            return "Multiple choices";
        case HTTP_STATUS_MOVED_PERMANENTLY:
            return "Moved permanently";
        case HTTP_STATUS_FOUND:
            return "Found";
        case HTTP_STATUS_SEE_OTHER:
            return "See other";
        case HTTP_STATUS_NOT_MODIFIED:
            return "Not modified";
        case HTTP_STATUS_USE_PROXY:
            return "Use proxy";
        case HTTP_STATUS_UNUSED:
            return "Unused";
        case HTTP_STATUS_TEMPORARY_REDIRECT:
            return "Temproary redirect";
        case HTTP_STATUS_PERMANENT_REDIRECT:
            return "Permanent redirect";
        case HTTP_STATUS_BAD_REQUEST:
            return "Bad request";
        case HTTP_STATUS_UNAUTHORIZED:
            return "Unauthorized";
        case HTTP_STATUS_PAYMENT_REQUIRED:
            return "Payment required";
        case HTTP_STATUS_FORBIDDEN:
            return "Forbidden";
        case HTTP_STATUS_NOT_FOUND:
            return "Not found";
        case HTTP_STATUS_METHOD_NOT_ALLOWED:
            return "Method not allowed";
        case HTTP_STATUS_NOT_ACCEPTABLE:
            return "Not acceptable";
        case HTTP_STATUS_PROXY_AUTHENTICATION_REQUIRED:
            return "Proxy authentication required";
        case HTTP_STATUS_REQUEST_TIMEOUT:
            return "Request timeout";
        case HTTP_STATUS_CONFLICT:
            return "Conflict";
        case HTTP_STATUS_GONE:
            return "Gone";
        case HTTP_STATUS_LENGTH_REQUIRED:
            return "Length required";
        case HTTP_STATUS_PRECONDITION_FAILED:
            return "Precondition failed";
        case HTTP_STATUS_PAYLOAD_TOO_LARGE:
            return "Payload too large";
        case HTTP_STATUS_URI_TOO_LONG:
            return "URI too long";
        case HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE:
            return "Unsupported media type";
        case HTTP_STATUS_RANGE_NOT_SATISFIABLE:
            return "Range not satisfiable";
        case HTTP_STATUS_EXPECTATION_FAILED:
            return "Expectation failed";
        case HTTP_STATUS_I_M_A_TEAPOT:
            return "I'm a teapot";
        case HTTP_STATUS_MISDIRECTED_REQUEST:
            return "Misdirected request";
        case HTTP_STATUS_UNPROCESSABLE_ENTITY:
            return "Unprocessable entity";
        case HTTP_STATUS_LOCKED:
            return "Locked";
        case HTTP_STATUS_FAILED_DEPENDANCY:
            return "Failed dependancy";
        case HTTP_STATUS_TOO_EARLY:
            return "Too early";
        case HTTP_STATUS_UPGRADE_REQUIRED:
            return "Upgrade required";
        case HTTP_STATUS_PRECONDITION_REQUIRED:
            return "Precondition required";
        case HTTP_STATUS_TOO_MANY_REQUESTS:
            return "Too many requests";
        case HTTP_STATUS_REQUEST_HEADER_FIELDS_TOO_LARGE:
            return "Request header fields too large";
        case HTTP_STATUS_UNAVAILABLE_FOR_LEGAL_REASONS:
            return "Unavailable for legal reasons";
        case HTTP_STATUS_INTERNAL_SERVER_ERROR:
            return "Internal server error";
        case HTTP_STATUS_NOT_IMPLEMENTED:
            return "Not implemented";
        case HTTP_STATUS_BAD_GATEWAY:
            return "Bad gateway";
        case HTTP_STATUS_SERVICE_UNAVAILABLE:
            return "Service unavailable";
        case HTTP_STATUS_GATEWAY_TIMEOUT:
            return "Gateawy timeout";
        case HTTP_STATUS_HTTP_VERSION_NOT_SUPPORTED:
            return "Http version not supported";
        case HTTP_STATUS_VARIANT_ALSO_NEGOTIATES:
            return "Variant also negotiates";
        case HTTP_STATUS_INSUFFICIENT_STORAGE:
            return "Insufficient storage";
        case HTTP_STATUS_LOOP_DETECTED:
            return "Loop detected";
        case HTTP_STATUS_NOT_EXTENDED:
            return "Not extended";
        case HTTP_STATUS_NETWORK_AUTHENTICATION_REQUIRED:
            return "Network authentication required";
        default:
            return "Unknown status code!";
    }
}

