#include "http_request_context.h"
#include "uri.h"

#include <string.h>

void init_request_context(Http_request_context_t* req_con) {
    init_http_requst(&req_con->req);
    req_con->status = PARSING_FINE;
    req_con->path = NULL;
    req_con->query = NULL;
}

void free_request_context(Http_request_context_t* req_con) {
    free_http_request(&req_con->req);
}

void clean_request_context(Http_request_context_t* req_con) {
    clean_http_request(&req_con->req);
    req_con->status = PARSING_FINE;
    req_con->path = NULL;
    req_con->query = NULL;
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

Http_status_t validate_target(Http_request_context_t* req_con) {
    //asterisk-form
    if (strcmp("*", req_con->req.request_line.target) == 0) {
        if (strcmp("OPTIONS", req_con->req.request_line.method) == 0) {
            return REQUEST_PROCESSING_FINE;
        }
        return HTTP_STATUS_BAD_REQUEST;
    }
    else if (is_valid_authority_form(req_con->req.request_line.target)) {
        if (strcmp("CONNECT", req_con->req.request_line.method) == 0) {
            return REQUEST_PROCESSING_FINE;
        }
        return HTTP_STATUS_BAD_REQUEST;
    }
    else if (is_valid_absoulte_form(req_con->req.request_line.target)) {
        return HTTP_STATUS_BAD_REQUEST; //Do not act as a proxy for now
    }
    else if (is_valid_origin_form(req_con->req.request_line.target)) {
        if (    strcmp("CONNECT", req_con->req.request_line.method) == 0 ||
                strcmp("OPTIONS", req_con->req.request_line.method) == 0) {
            return HTTP_STATUS_BAD_REQUEST;
        }
        int i = 0;
        while ( req_con->req.request_line.target[i] != '?' &&
                req_con->req.request_line.target[i] != '\0')
            ++i;
        if (req_con->req.request_line.target[i] == '?') {
            req_con->req.request_line.target[i] = '\0';
            ++i;
        }
        req_con->path = req_con->req.request_line.target;
        req_con->query = &req_con->req.request_line.target[i];
        return REQUEST_PROCESSING_FINE;
    }
    return HTTP_STATUS_BAD_REQUEST;
}

Http_status_t process_rquest_line(Http_request_context_t* req_con) {
    if (strcmp(req_con->req.request_line.method, "UNKNOWN METHOD") == 0)
        return HTTP_STATUS_BAD_REQUEST;
    if (strcmp(req_con->req.request_line.version, "UNSUPPORTED VERSION") == 0)
        return HTTP_STATUS_HTTP_VERSION_NOT_SUPPORTED;
    return validate_target(req_con->req.request_line.target);
}

Http_status_t process_request(Http_request_context_t* req_con) {
    //If there was an error before this phase, return immediately.
    if (req_con->status != PARINSG_FINE) return req_con->status;

    req_con->status = REQUEST_PROCESSING_FINE;

    //Populate the below array with processing stage functions in order those stages should run
    //if stage would return status different than REQUEST_PROCESSING_FINE
    //loop stops immediately and status of last processed stage is returned
    typedef Http_status_t (*processing_stage_func)(Http_request_context_t* req_con);
    const processing_stage_func processing_stages[] = {
        process_request_line
    };

    for (   size_t i = 0;
            i < sizeof(processing_stages)/sizeof(processing_stages[0]) &&
            req_con->status == REQUEST_PROCESSING_FINE;
            ++i) {
        req_con->status = processing_stages[i](req_con);
    }
    return req_con->status;
}

