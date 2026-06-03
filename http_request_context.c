#include "http_request_context.h"
#include "uri_parser.h"
#include "uri.h"

#include <string.h>
#include <sys/types.h>
#include <stdio.h>

void init_request_context(Http_request_context_t* req_con) {
    init_http_request(&req_con->req);
    req_con->status = PARSING_FINE;
    memset(&req_con->uri, 0, sizeof(req_con->uri));
    req_con->close_connection_after_response = 0;
    req_con->body_chunked = 0;
    req_con->body_size = 0;
}

void free_request_context(Http_request_context_t* req_con) {
    free_http_request(&req_con->req);
}

void clean_request_context(Http_request_context_t* req_con) {
    clean_http_request(&req_con->req);
    req_con->status = PARSING_FINE;
    memset(&req_con->uri, 0, sizeof(req_con->uri));
    req_con->close_connection_after_response = 0;
    req_con->body_chunked = 0;
    req_con->body_size = 0;
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
        if (parse_relative_ref(req_con->req.request_line.target, &uri) == URI_PARSING_FAIL)
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

Http_status_t process_request_line(Http_request_context_t* req_con) {
    if (strcmp(req_con->req.request_line.method, "UNKNOWN METHOD") == 0)
        return HTTP_STATUS_NOT_IMPLEMENTED;
    if (strcmp(req_con->req.request_line.version, "UNSUPPORTED VERSION") == 0)
        return HTTP_STATUS_HTTP_VERSION_NOT_SUPPORTED;
    return validate_target(req_con);
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

//RFC 9112 3.3
Http_status_t process_scheme(Http_request_context_t* req_con) {
    //set or check scheme
    if (!req_con->uri.scheme.len) {
        req_con->uri.scheme.cstr = "http";
        req_con->uri.scheme.len = 4;
    }
    else {
        //support http scheme only
        if (    req_con->uri.scheme.len != 4 ||
                req_con->uri.scheme.cstr[0] != 'h' ||
                req_con->uri.scheme.cstr[1] != 't' ||
                req_con->uri.scheme.cstr[2] != 't' ||
                req_con->uri.scheme.cstr[3] != 'p')
            return HTTP_STATUS_BAD_REQUEST;
    }
    return REQUEST_PROCESSING_FINE;
}

Http_status_t process_transfer_encoding_line(Http_request_context_t* req_con) {
    Field_line_t* trenc_fl = find_field_line_in_hash_map(&req_con->req.headers, "Transfer-Encoding");
    //if there is no "Transfer-Encoding" or it is empty, skip this stage
    if (!trenc_fl || trenc_fl->field_values[0][0] == '\0') return REQUEST_PROCESSING_FINE;
    Field_line_t* cl_fl = find_field_line_in_hash_map(&req_con->req.headers, "Content-Length");
    //If there were both "Transfer-Encoding" and "Content-Length"
    //process this request, but close connection afterward
    if (cl_fl) req_con->close_connection_after_response = 1;
    //for now support only "chunked", so every other encoding is unsupported for now
    //we should return 501 HTTP_STATUS_NOT_IMPLEMENTED
    char* str = trenc_fl->field_values[0];
    if (str[0] != 'c' ||
        str[1] != 'h' ||
        str[2] != 'u' ||
        str[3] != 'n' ||
        str[4] != 'k' ||
        str[5] != 'e' ||
        str[6] != 'd')
        return HTTP_STATUS_NOT_IMPLEMENTED;
    req_con->body_chunked = 1;
    return REQUEST_PROCESSING_FINE;
}

Http_status_t process_content_length_line(Http_request_context_t* req_con) {
    //if there were "Transfer-Encoding", nothing to check here
    Field_line_t* trenc_fl = find_field_line_in_hash_map(&req_con->req.headers, "Transfer-Encoding");
    if (trenc_fl && trenc_fl->field_values[0][0] != '\0') return REQUEST_PROCESSING_FINE;
    //if there were no "Content-Length", nothing to check here
    Field_line_t* cl_fl = find_field_line_in_hash_map(&req_con->req.headers, "Content-Length");
    if (!cl_fl) return REQUEST_PROCESSING_FINE;
    char* ptr = cl_fl->field_values[0];
    size_t cl = 0;
    do { //at this point we know that ptr is a string of digits
         //we checked it during parsing request
        cl *= 10;
        cl += (*ptr - '0');
        ++ptr;
    } while (*ptr != '\0');
    req_con->body_size = cl;
    return REQUEST_PROCESSING_FINE;
}

Http_status_t process_request(Http_request_context_t* req_con) {
    //If there was an error before this phase, return immediately.
    if (req_con->status != PARSING_FINE) return req_con->status;

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
        process_request_line,
        process_scheme,
        process_transfer_encoding_line,
        process_content_length_line
    };

    for (   size_t i = 0;
            i < sizeof(processing_stages)/sizeof(processing_stages[0]) &&
            req_con->status == REQUEST_PROCESSING_FINE;
            ++i) {
        req_con->status = processing_stages[i](req_con);
    }
    return req_con->status;
}

void print_request_context(Http_request_context_t* req_con) {
    Http_request_t req = req_con->req;
    printf( "%s %s %s\n",
            req.request_line.method,
            req.request_line.target,
            req.request_line.version);
    for (size_t i = 0; i < req.headers.capacity; ++i) {
        Field_line_hash_map_bucket_t* b = &req.headers.buckets[i];
        if (b->bucket_status != OCCUPIED) continue;
        for (size_t j = 0; j < b->field_line.count; ++j) {
            printf("%s:%s\n",
                    b->field_line.field_name,
                    b->field_line.field_values[j]);
        }
    }
    printf("\n");
#define MAX_SIZE_REQUEST_BODY_TO_LOG (32 * 1024)
    if (req.body_size) {
        if (req.body_size <= MAX_SIZE_REQUEST_BODY_TO_LOG) {
            printf("%.*s\n", (int)req.body_size, req.body);
        }
        else {
            printf("Body too large to print!\n");
        }
    }
    if (req.has_trailers_section) {
        for (size_t i = 0; i < req.trailers.capacity; ++i) {
            Field_line_hash_map_bucket_t* b = &req.trailers.buckets[i];
            if (b->bucket_status != OCCUPIED) continue;
            for (size_t j = 0; j < b->field_line.count; ++j) {
                printf("%s:%s\n",
                        b->field_line.field_name,
                        b->field_line.field_values[j]);
            }
        }
        printf("\n");
    }
}

