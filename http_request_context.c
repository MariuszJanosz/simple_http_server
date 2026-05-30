#include "http_request_context.h"
#include "uri_parser.h"

#include <string.h>

void init_request_context(Http_request_context_t* req_con) {
    init_http_requst(&req_con->req);
    req_con->status = PARSING_FINE;
    memset(&req_con->uri, 0, sizeof(req_con->uri));
}

void free_request_context(Http_request_context_t* req_con) {
    free_http_request(&req_con->req);
}

void clean_request_context(Http_request_context_t* req_con) {
    clean_http_request(&req_con->req);
    req_con->status = PARSING_FINE;
    memset(&req_con->uri, 0, sizeof(req_con->uri));
}

Http_status_t validate_target(Http_request_context_t* req_con) {
    if (strcmp("*", req_con->req.request_line.target) == 0) { //asterisk-form or invalid
        if (strcmp("OPTIONS", req_con->req.request_line.method) == 0) {
            return REQUEST_PROCESSING_FINE;
        }
        return HTTP_STATUS_BAD_REQUEST;
    }
    else if (req_con->req.request_line.target[0] == '/') { //origin-form or invalid
        if (    strcmp("CONNECT", req_con->req.request_line.method) == 0 ||
                strcmp("OPTIONS", req_con->req.request_line.method) == 0)
            return HTTP_STATUS_BAD_REQUEST;
        URI uri;
        if (parse_relative_ref(req_con.req.request_line.target, &uri) == URI_PARSING_FAIL)
            return HTTP_STATUS_BAD_REQUEST;
        if (    uri.scheme.len || uri.userinfo.len ||
                uri.host.len || uri.port.len || uri.fragment.len)
            return HTTP_STATUS_BAD_REQUEST;
        if (!uri_is_path_absolute(uri.path.cstr, uri.path.len))
            return HTTP_STATUS_BAD_REQUEST;
        if (!uri_is_query(uri.query.cstr, uri.query.len));
            return HTTP_STATUS_BAD_REQUEST;
        //If all checks passed set path and query and return
        req_con->uri.path = uri.path;
        req_con->uri.query = uri.query;
        return REQUEST_PROCESSING_FINE;
    }
    else { //absolute-form, authority-form or invalid
        URI uri;
        if (parse_URI(req_con->req.request_line.target, &uri) == URI_PARSING_FAIL) {
            //authority-form or invalid
            if (strcmp("CONNECT", req_con->req.request_line.method) != 0)
                return HTTP_STATUS_BAD_REQUEST;
            const char* cstr = req_con->req.request_line.target;
            StringView host, port;
            host.cstr = cstr;
            ssize_t last_colon = -1;
            size_t i = 0;
            while (cstr[i] != '\0') {
                if (cstr[i] == ':') last_colon = i;
                ++i;
            }
            if (last_colon == -1)
                return HTTP_STATUS_BAD_REQUEST;
            host.len = last_colon;
            port.cstr = cstr + last_colon + 1;
            port.len = i - (last_colon + 1);
            if (    !uri_is_host(host.cstr, host.len) ||
                    !uri_is_port(port.cstr, port.len))
                return HTTP_STATUS_BAD_REQUEST;
            //check if host and port of authority-form match those from host field line
            if (    host.len != req_con->uri.host.len ||
                    port.len != req_con->uri.port.len)
                return HTTP_STATUS_BAD_REQUEST;
            for (size_t i = 0; i < host.len; ++i) {
                if (host.cstr[i] != req_con->uri.host.cstr[i])
                    return HTTP_STATUS_BAD_REQUEST;
            }
            for (size_t i = 0; i < port.len; ++i) {
                if (port.cstr[i] != req_con->uri.port.cstr[i])
                    return HTTP_STATUS_BAD_REQUEST;
            }
            return REQUEST_PROCESSING_FINE;
        }
        else { //absolute-form or invalid
            if (    strcmp("CONNECT", req_con->req.request_line.method) == 0 ||
                    strcmp("OPTIONS", req_con->req.request_line.method) == 0)
                return HTTP_STATUS_BAD_REQUEST;
            if (uri.fragment.len)
                return HTTP_STATUS_BAD_REQUEST;
            req_con->uri.scheme = uri.scheme;
            if (uri.userinfo.len) req_con->uri.userinfo = uri.userinfo;
            if (uri.host.len) req_con->uri.host = uri.host;
            if (uri.port.len) req_con->uri.port = uri.port;
            req_con->uri.path = uri.path;
            req_con->uri.query = uri.query;
            return REQUEST_PROCESSING_FINE;
        }
    }
}

Http_status_t process_rquest_line(Http_request_context_t* req_con) {
    if (strcmp(req_con->req.request_line.method, "UNKNOWN METHOD") == 0)
        return HTTP_STATUS_NOT_IMPLEMENTED;
    if (strcmp(req_con->req.request_line.version, "UNSUPPORTED VERSION") == 0)
        return HTTP_STATUS_HTTP_VERSION_NOT_SUPPORTED;
    return validate_target(req_con->req.request_line.target);
}

Http_status_t process_host_field_line(Http_request_context_t* req_con) {
    Field_line_t* host_fl = find_field_line_in_hash_map(&req_con->req.headers, "Host");
    if (!host_fl) return HTTP_STATUS_BAD_REQUEST;
    const char* cstr = host_fl->field_values[0];
    size_t i = 0;
    ssize_t last_colon = -1;
    StringView host, port;
    if (cstr[i] == '[') {
        while (cstr[i] != ']' && cstr[i] != '\0') ++i;
    }
    while (cstr[i] != '\0') {
        if (cstr[i] == ':') last_colon = i;
        ++i;
    }
    if (last_colon == -1) {
        host.cstr = cstr;
        host.len = i;
        //If there is no port set it to default
        port.cstr = "80";
        port.len = 2;
    }
    else {
        host.cstr = cstr;
        host.len = last_colon;
        port.cstr = cstr + last_colon + 1;
        port.len = i - (last_colon + 1);
    }
    if (    !uri_is_host(host.cstr, host.len) ||
            !uri_is_port(port.cstr, port.len))
        return HTTP_STATUS_BAD_REQUEST;
    req_con->uri.host = host;
    req_con->uri.port = port;
    return REQUEST_PROCESSING_FINE;
}

Http_status_t process_request(Http_request_context_t* req_con) {
    //If there was an error before this phase, return immediately.
    if (req_con->status != PARINSG_FINE) return req_con->status;

    req_con->status = REQUEST_PROCESSING_FINE;

    //Populate the below array with processing stage functions in order those stages should run
    //if stage would return status different than REQUEST_PROCESSING_FINE
    //loop stops immediately and status of last processed stage is returned
    //keep processing stages in topological order according to dependencies DAG
    //for example if request target checked in process_request_line is in authority-form
    //its host and port must match those from host field line,
    //so process_host_field_line shoul run earlier to provide required context
    typedef Http_status_t (*processing_stage_func)(Http_request_context_t* req_con);
    const processing_stage_func processing_stages[] = {
        process_host_field_line,
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

