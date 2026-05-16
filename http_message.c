#include "http_message.h"
#include "log.h"
#include "reader.h"
#include "uri.h"
#include "abnf.h"
#include "tcp_connection.h"
#include "http_field_line.h"

#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <assert.h>

#include <unistd.h>

void init_http_message(Http_message_t* http_msg, Message_type_t type) {
    http_msg->message_type = type;
    http_msg->start_line = NULL;
    init_field_line_hash_map(&http_msg->field_line_hash_map, 32);
    http_msg->message_body = NULL;
    http_msg->body_size = 0;
    http_msg->body_fd = -1;
}

void free_http_message(Http_message_t* http_msg) {
    if (http_msg->start_line) {
        if (http_msg->message_type == HTTP_REQUEST) {
            Request_line_t* rl = http_msg->start_line;
            if (rl->request_target) {
                free(rl->request_target);
            }
        }
        free(http_msg->start_line);
    }
    free_field_line_hash_map(&http_msg->field_line_hash_map);
    if (http_msg->message_body) {
        free(http_msg->message_body);
    }
    if (http_msg->body_fd != -1) {
        close(http_msg->body_fd);
    }
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
    size_t len = strlen(str);
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
    size_t len = strlen(str);
    if (str[i] == '[') {
        while (str[i] != '\0' && str[i] != ']') {
            ++i;
        }
    }
    while (i < len) {
        if (0x3A == str[i] /*:*/) {
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

Http_status_t parse_request_line(Http_message_t* http_msg, Tcp_connection_t tcp_con) {
again:
    char* input = get_line(tcp_con);
    if (!input || input[0] == '\0') {
        return HTTP_STATUS_BAD_REQUEST;
    }
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
    size_t l = strlen(http_version);
    if (l > 0 && http_version[l - 1] == '\r') {
        http_version[l - 1] = '\0';
    }
    if (!is_valid_http_version(http_version)) {
        LOG(INFO, "Invalid http version!");
        free(input);
        free(rl.request_target);
        return HTTP_STATUS_HTTP_VERSION_NOT_SUPPORTED;
    }
    else {
        rl.http_version = "HTTP/1.1";
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

Http_status_t parse_field_line(Http_message_t* http_msg, Tcp_connection_t tcp_con, int *is_empty) {
    char* input = get_line(tcp_con);
    if (!input) {
        *is_empty = 1;
        return HTTP_STATUS_BAD_REQUEST;
    }
    // RFC9112 2.2 field lines starting with a white space shall be ignored
    if (isspace(input[0])) {
        if ((input[0] == '\r' && input[1] == '\n') || input[0] == '\n') {
            *is_empty = 1;
        }
        free(input);
        return HTTP_STATUS_OK;
    }
    char* name = strtok(input, ":");
    char* value = strtok(NULL, "\n");
    if (!name || !value) {
        LOG(INFO, "Invalid field line, to few arguments!");
        free(input);
        return HTTP_STATUS_BAD_REQUEST;
    }
    Field_line_t fl;
    fl.field_name = name;
    if (!is_valid_field_name(fl.field_name)) {
        LOG(INFO, "Invalid field name!");
        free(input);
        free(fl.field_name);
        return HTTP_STATUS_BAD_REQUEST;
    }
    size_t l = strlen(value);
    char* tmp = value;
    while (l > 0 && (tmp[l - 1] == '\r' || tmp[l - 1] == ' ')) {
        l -= 1;
        tmp[l] = '\0';
    }
    while (*tmp == ' ') {
        tmp += 1;
    }
    fl.field_value = tmp;
    if (!is_valid_field_value(fl.field_value)) {
        LOG(INFO, "Invalid field value!");
        free(input);
        free(fl.field_name);
        free(fl.field_value);
        return HTTP_STATUS_BAD_REQUEST;
    }
    add_field_line_to_hash_map(&http_msg->field_line_hash_map, fl);
    free(input);
    return HTTP_STATUS_OK;
}

Http_status_t read_body_cl(Http_message_t* http_msg, Tcp_connection_t tcp_con, size_t content_length) {
    unsigned char* body_data = malloc(content_length * sizeof(*body_data));
    if (!body_data) {
        LOG(ERROR, "malloc failed!");
        exit(1);
    }
    size_t read = 0;
    while (read < content_length) {
        unsigned char* first_empty = body_data + read;
        size_t to_read = content_length - read;
        size_t r = get_data(tcp_con, first_empty, to_read);
        if (r == 0) { //EOF before entire content was transfered
            http_msg->message_body = body_data;
            http_msg->body_size = read;
            return HTTP_STATUS_BAD_REQUEST;
        }
        read += r;
    }
    http_msg->message_body = body_data;
    http_msg->body_size = content_length;
    return HTTP_STATUS_OK;
}

Http_status_t read_body_chunked(Http_message_t* http_msg, Tcp_connection_t tcp_con) {
    size_t data_capacity = 1024; //1kB
    unsigned char* body_data = malloc(data_capacity * sizeof(*body_data));
    size_t data_first_free_index = 0;
    if (!body_data) {
        LOG(ERROR, "malloc failed!");
        exit(1);
    }
    int finished = 0;
    while (!finished) {
        char* size = get_line(tcp_con);
        if (!size) {
            free(body_data);
            return HTTP_STATUS_BAD_REQUEST;
        }
        size_t len = strlen(size);
        if (size[len - 1] == '\n') {
            size[--len] = '\0'; // '\n'->'\0'
        }
        else {
            free(size);
            free(body_data);
            return HTTP_STATUS_BAD_REQUEST;
        }
        if (size[len - 1] == '\r') {
            size[--len] = '\0'; // '\r'->'\0'
        }
        if (!abnf_is_HEXDIG(size[0])) {
            free(size);
            free(body_data);
            return HTTP_STATUS_BAD_REQUEST;
        }
        char *end;
        uintmax_t content_length = strtoumax(size, &end, 16);
        if (*end != '\0') {
            free(size);
            free(body_data);
            return HTTP_STATUS_BAD_REQUEST;
        }
        else if (content_length > MAX_BODY_SIZE) {
            free(size);
            free(body_data);
            return HTTP_STATUS_PAYLOAD_TOO_LARGE;
        }
        else if (content_length == 0) {
            finished = 1;
        }
        else {
            Http_message_t fake_tmp;
            fake_tmp.message_body = NULL;
            //If we are here 0 < content_length < MAX_BODY_SIZE
            //we can safely cast to size_t
            read_body_cl(&fake_tmp, tcp_con, (size_t)content_length);
            if (content_length > (data_capacity - data_first_free_index)) {
                while (content_length > (data_capacity - data_first_free_index)) {
                    data_capacity *= 2;
                }
                unsigned char* tmp = realloc(body_data, data_capacity * sizeof(*body_data));
                if (!tmp) {
                    LOG(ERROR, "realloc failed!");
                    exit(1);
                }
                body_data = tmp;
            }
            memcpy(body_data + data_first_free_index, fake_tmp.message_body, content_length);
            data_first_free_index += content_length;
            free(fake_tmp.message_body);
        }
        free(size);
        //Flush remaining \r\n
        char* tmp = get_line(tcp_con);
        if (!tmp) {
            free(body_data);
            return HTTP_STATUS_BAD_REQUEST;
        }
        free(tmp);
    }
    http_msg->message_body = body_data;
    http_msg->body_size = data_first_free_index;
    return HTTP_STATUS_OK;
}

//For now it works only for requests like Transfer-Encoding: chunked but not for Transfer-Encoding:gzip,chunked
Http_status_t read_body(Http_message_t* http_msg, Tcp_connection_t tcp_con) {
    Field_line_t* TEFL =
        find_field_line_in_hash_map(&http_msg->field_line_hash_map, "Transfer-Encoding");
    if (TEFL) {
        char* transfer_encoding_val = TEFL->field_value;
        if (strcmp(transfer_encoding_val, "chunked") == 0) {
            return read_body_chunked(http_msg, tcp_con);
        }
    }
    Field_line_t* CLFL =
        find_field_line_in_hash_map(&http_msg->field_line_hash_map, "Content-Length");
    if (CLFL) {
        char* str = CLFL->field_value;
        if (!abnf_is_DIGIT(str[0])) {
            return HTTP_STATUS_BAD_REQUEST;
        }
        char* end;
        uintmax_t content_length = strtoumax(str, &end, 10);
        if (*end != '\0') {
            return HTTP_STATUS_BAD_REQUEST;
        }
        if (content_length > MAX_BODY_SIZE) {
            return HTTP_STATUS_PAYLOAD_TOO_LARGE;
        }
        //If we are here 0 <= content_length <= MAX_BODY_SIZE
        //we can safely cast to size_t
        return read_body_cl(http_msg, tcp_con, (size_t)content_length);
    }
    return HTTP_STATUS_OK; //There is no body
}

Http_status_t host_field_matches_request_target(Http_message_t* http_msg) {
    Field_line_t* HFL = find_field_line_in_hash_map(&http_msg->field_line_hash_map, "Host");
    if (!HFL) { //Host is required
        return HTTP_STATUS_BAD_REQUEST;
    }
    else {
        char* host_value = HFL->field_value;
        char* request_target = ((Request_line_t*)(http_msg->start_line))->request_target;
        if (is_valid_authority_form(request_target)) {
            int i = 0;
            while (request_target[i] != '@' && request_target[i] != '\0') {
                i += 1;
            }
            if (request_target[i] == '@') { /*host-uri = request_target[i+1:]*/
                if (strcmp(host_value, &request_target[i + 1]) != 0) {
                    return HTTP_STATUS_BAD_REQUEST;
                }
            }
            else { /*host-uri = request_target[:]*/
                if (strcmp(host_value, request_target) != 0) {
                    return HTTP_STATUS_BAD_REQUEST;
                }
            }
        }
        else if (is_valid_absolute_form(request_target)) {
            int i = 0;
            while (request_target[i] != ':') {
                i += 1;
            }
            i += 1; //Now we are past scheme ":"
            if (request_target[i] == '/' && request_target[i + 1] == '/') {
                i += 2;
                /*now i points to the start of authority*/
                int j = i;
                while ( request_target[j] != '?' &&
                        request_target[j] != '@' &&
                        request_target[j] != '\0') {
                    j += 1;
                }
                if (request_target[j] == '?') { /*host-uri = request_target[i:j]*/
                    request_target[j] = '\0';
                    if (strcmp(host_value, &request_target[i]) != 0) {
                        return HTTP_STATUS_BAD_REQUEST;
                    }
                    request_target[j] = '?';
                }
                else if (request_target[j] == '@') {
                    j += 1;
                    i = j; /*now i points to the start of host-uri*/
                    while (request_target[j] != '?' && request_target[j] != '\0') {
                        j += 1;
                    }/*host-uri = request_target[i:j]*/
                    if (request_target[j] == '\0') {
                        if (strcmp(host_value, &request_target[i]) != 0) {
                            return HTTP_STATUS_BAD_REQUEST;
                        }
                    }
                    else {
                        request_target[j] = '\0';
                        if (strcmp(host_value, &request_target[i]) != 0) {
                            return HTTP_STATUS_BAD_REQUEST;
                        }
                        request_target[j] = '?';
                    }
                }
                else {/*host-uri = request_target[i:]*/
                    if (strcmp(host_value, &request_target[i]) != 0) {
                        return HTTP_STATUS_BAD_REQUEST;
                    }
                }
            }
            else { //there is no authority in hier-part, host field should have empty value
                if (strcmp(host_value, "") != 0) {
                    return HTTP_STATUS_BAD_REQUEST;
                }
            }
        }
        else if (is_valid_origin_form(request_target)) {
            //In origin-form target URIs host is derived from host field line,
            //so we don't have to do anything
        }
        else {//It is asterisk-form
            //TODO
        }
    }
    
    return HTTP_STATUS_OK;
}

Http_status_t parse_http_request(Http_message_t* http_msg, Tcp_connection_t tcp_con) {
    Http_status_t status = parse_request_line(http_msg, tcp_con);
    int is_empty = 0;
    while (!is_empty) {
        if (status == HTTP_STATUS_OK) {
            status = parse_field_line(http_msg, tcp_con, &is_empty);
        }
        else {
            parse_field_line(http_msg, tcp_con, &is_empty);
        }
    }
    if (status == HTTP_STATUS_OK) {
        status = read_body(http_msg, tcp_con);
    }
    else {
        read_body(http_msg, tcp_con);
    }
    if (status == HTTP_STATUS_OK) {
        status = host_field_matches_request_target(http_msg);
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

const char* http_status_to_string_num(Http_status_t status) {
    switch (status) {
        case HTTP_STATUS_CONTINUE:
            return "100";
        case HTTP_STATUS_SWITCHING_PROTOCOLS:
            return "101";
        case HTTP_STATUS_PROCESSING:
            return "102";
        case HTTP_STATUS_EARLY_HINTS:
            return "103";
        case HTTP_STATUS_OK:
            return "200";
        case HTTP_STATUS_CREATED:
            return "201";
        case HTTP_STATUS_ACCEPTED:
            return "202";
        case HTTP_STATUS_NON_AUTHORITATIVE_INFORMATION:
            return "203";
        case HTTP_STATUS_NO_CONTENT:
            return "204";
        case HTTP_STATUS_RESET_CONTENT:
            return "205";
        case HTTP_STATUS_PARTIAL_CONTENT:
            return "206";
        case HTTP_STATUS_MULTI_STATUS:
            return "207";
        case HTTP_STATUS_ALREADY_REPORTED:
            return "208";
        case HTTP_STATUS_IM_USED:
            return "226";
        case HTTP_STATUS_MULTIPLE_CHOICES:
            return "300";
        case HTTP_STATUS_MOVED_PERMANENTLY:
            return "301";
        case HTTP_STATUS_FOUND:
            return "302";
        case HTTP_STATUS_SEE_OTHER:
            return "303";
        case HTTP_STATUS_NOT_MODIFIED:
            return "304";
        case HTTP_STATUS_USE_PROXY:
            return "305";
        case HTTP_STATUS_UNUSED:
            return "306";
        case HTTP_STATUS_TEMPORARY_REDIRECT:
            return "307";
        case HTTP_STATUS_PERMANENT_REDIRECT:
            return "308";
        case HTTP_STATUS_BAD_REQUEST:
            return "400";
        case HTTP_STATUS_UNAUTHORIZED:
            return "401";
        case HTTP_STATUS_PAYMENT_REQUIRED:
            return "402";
        case HTTP_STATUS_FORBIDDEN:
            return "403";
        case HTTP_STATUS_NOT_FOUND:
            return "404";
        case HTTP_STATUS_METHOD_NOT_ALLOWED:
            return "405";
        case HTTP_STATUS_NOT_ACCEPTABLE:
            return "406";
        case HTTP_STATUS_PROXY_AUTHENTICATION_REQUIRED:
            return "407";
        case HTTP_STATUS_REQUEST_TIMEOUT:
            return "408";
        case HTTP_STATUS_CONFLICT:
            return "409";
        case HTTP_STATUS_GONE:
            return "410";
        case HTTP_STATUS_LENGTH_REQUIRED:
            return "411";
        case HTTP_STATUS_PRECONDITION_FAILED:
            return "412";
        case HTTP_STATUS_PAYLOAD_TOO_LARGE:
            return "413";
        case HTTP_STATUS_URI_TOO_LONG:
            return "414";
        case HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE:
            return "415";
        case HTTP_STATUS_RANGE_NOT_SATISFIABLE:
            return "416";
        case HTTP_STATUS_EXPECTATION_FAILED:
            return "417";
        case HTTP_STATUS_I_M_A_TEAPOT:
            return "418";
        case HTTP_STATUS_MISDIRECTED_REQUEST:
            return "421";
        case HTTP_STATUS_UNPROCESSABLE_ENTITY:
            return "422";
        case HTTP_STATUS_LOCKED:
            return "423";
        case HTTP_STATUS_FAILED_DEPENDANCY:
            return "424";
        case HTTP_STATUS_TOO_EARLY:
            return "425";
        case HTTP_STATUS_UPGRADE_REQUIRED:
            return "426";
        case HTTP_STATUS_PRECONDITION_REQUIRED:
            return "428";
        case HTTP_STATUS_TOO_MANY_REQUESTS:
            return "429";
        case HTTP_STATUS_REQUEST_HEADER_FIELDS_TOO_LARGE:
            return "431";
        case HTTP_STATUS_UNAVAILABLE_FOR_LEGAL_REASONS:
            return "451";
        case HTTP_STATUS_INTERNAL_SERVER_ERROR:
            return "500";
        case HTTP_STATUS_NOT_IMPLEMENTED:
            return "501";
        case HTTP_STATUS_BAD_GATEWAY:
            return "502";
        case HTTP_STATUS_SERVICE_UNAVAILABLE:
            return "503";
        case HTTP_STATUS_GATEWAY_TIMEOUT:
            return "504";
        case HTTP_STATUS_HTTP_VERSION_NOT_SUPPORTED:
            return "505";
        case HTTP_STATUS_VARIANT_ALSO_NEGOTIATES:
            return "506";
        case HTTP_STATUS_INSUFFICIENT_STORAGE:
            return "507";
        case HTTP_STATUS_LOOP_DETECTED:
            return "508";
        case HTTP_STATUS_NOT_EXTENDED:
            return "510";
        case HTTP_STATUS_NETWORK_AUTHENTICATION_REQUIRED:
            return "511";
        default:
            return "500";
    }
}

void write_response_status_line(Http_message_t* http_msg, const char* http_version, const char* status, const char* reason) {
    http_msg->start_line = malloc(sizeof(Status_line_t));
    if (!(http_msg->start_line)) {
        LOG(ERROR, "malloc failed!");
        exit(1);
    }
    ((Status_line_t*)(http_msg->start_line))->http_version = http_version;
    ((Status_line_t*)(http_msg->start_line))->status_code = status;
    ((Status_line_t*)(http_msg->start_line))->status_text = reason;
}

void write_response_field_line(Http_message_t* http_msg, char* field_name, char* field_value) {
    const Field_line_t fl = {field_name, field_value};
    add_field_line_to_hash_map(&http_msg->field_line_hash_map, fl);
}

void write_response_body_content_length(Http_message_t* http_msg, char* body, size_t content_length) {
    http_msg->message_body = body;
    http_msg->body_size = content_length;
}

size_t get_response_size(Http_message_t* http_msg) {
    size_t msg_size = 0;
    msg_size += strlen(((Status_line_t*)(http_msg->start_line))->http_version);
    msg_size += 1;//" "
    msg_size += strlen(((Status_line_t*)(http_msg->start_line))->status_code);
    msg_size += 1;//" "
    msg_size += strlen(((Status_line_t*)(http_msg->start_line))->status_text);
    msg_size += 2;//CRLF
    for (int i = 0; i < http_msg->field_line_hash_map.capacity; ++i) {
        if (http_msg->field_line_hash_map.buckets[i].bucket_status != OCCUPIED) continue;
        msg_size += strlen(http_msg->field_line_hash_map.buckets[i].field_line.field_name);
        msg_size += 1;//":"
        msg_size += strlen(http_msg->field_line_hash_map.buckets[i].field_line.field_value);
        msg_size += 2;//CRLF
    }
    msg_size += 2;//CRLF
    if (http_msg->body_size) {
        msg_size += http_msg->body_size;
    }
    return msg_size;
}

void send_buf(int fd, char* buf, size_t size) {
    size_t n = 0;
    while (n < size) {
        ssize_t tmp = write(fd, buf + n, size - n);
        if (tmp < 0) {
            LOG(INFO, "write failed!");
            break;
        }
        n += tmp;
    }
}

void send_response(Tcp_connection_t tcp_con, Http_message_t* http_msg) {
    size_t msg_size = get_response_size(http_msg);
    if (msg_size == 0) {
        return;
    }
    char* response = malloc(msg_size * sizeof(*response));
    if (!response) {
        LOG(ERROR, "malloc failed!");
        exit(1);
    }
    size_t count = 0;
    //Prepare status line
    count += sprintf(response + count, "%s %s %s\r\n",
                ((Status_line_t*)(http_msg->start_line))->http_version,
                ((Status_line_t*)(http_msg->start_line))->status_code,
                ((Status_line_t*)(http_msg->start_line))->status_text);
    //Prepare field lines
    for (int i = 0; i < http_msg->field_line_hash_map.capacity; ++i) {
        if (http_msg->field_line_hash_map.buckets[i].bucket_status != OCCUPIED) continue;
        count += sprintf(response + count, "%s:%s\r\n",
                http_msg->field_line_hash_map.buckets[i].field_line.field_name,
                http_msg->field_line_hash_map.buckets[i].field_line.field_value);
    }
    //CRLF
    count += sprintf(response + count, "\r\n");
    //Body
    if (http_msg->body_size) {
        memcpy(response + count, http_msg->message_body, http_msg->body_size);
        count += http_msg->body_size;
    }
    if (count < msg_size) {
        LOG(ERROR, "Faild to prepare message!");
        exit(1);
    }
    send_buf(tcp_con.fd, response, count);
    free(response);
}

void send_response_chunked(Tcp_connection_t tcp_con, Http_message_t* http_msg, int fd, Chunker_func_t chunker) {
    assert(http_msg->message_body == NULL && "message_body not NULL in send_response_chunked!");
    send_response(tcp_con, http_msg); //Send header
    char chunk[DEFAULT_CHUNK_SIZE + 16]; //+16 additional space for size and 2x\r\n
    size_t chunk_size;
    while (chunk_size = chunker(fd, chunk + 14)) {
        chunk[14 + chunk_size] = '\r';
        chunk[14 + chunk_size + 1] = '\n';
        chunk[13] = '\n';
        chunk[12] = '\r';
        int i = 12;
        size_t cs = chunk_size;
        static const char* digit_lookup = "0123456789ABCDEF";
        while (cs) {
            --i;
            assert(i >= 0 && "chunk too big!");
            chunk[i] = digit_lookup[cs % 16];
            cs /= 16;
        }
        send_buf(tcp_con.fd, chunk + i, chunk_size + 16 - i);
    }
    //EOF send closing chunk
    send_buf(tcp_con.fd, "0\r\n\r\n", strlen("0\r\n\r\n"));
}

size_t default_chunker(int fd, char* chunk) {
    ssize_t bytes_read = read(fd, chunk, DEFAULT_CHUNK_SIZE);
    if (bytes_read < 0) {
        LOG(ERROR, "read failed!");
        exit(1);
    }
    return bytes_read;
}

