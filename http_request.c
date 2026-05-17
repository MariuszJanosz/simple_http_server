#include "http_request.h"
#include "http_field_line.h"

#include <string.h>
#include <ctype.h>

void init_http_request(Http_request_t* req) {
    req->request_line.method = NULL;
    req->request_line.target = NULL;
    req->request_line.version = NULL;
    init_field_line_hash_map(&req->headers, 32);
    req->body = NULL;
    req->body_size = 0;
    init_field_line_hash_map(&req->trailers, 4);
}

void free_http_request(Http_request_t* req) {
    req->request_line.method = NULL;
    free(req->request_line.target);
    req->request_line.target = NULL;
    req->request_line.version = NULL;
    free_field_line_hash_map(&req->headers);
    free(req->body);
    req->body = NULL;
    req->body_size = 0;
    free_field_line_hash_map(&req->trailers);
}

const char* method_string_to_literal(const char* method) {
    if (!method) return "UNKNOWN METHOD";
    if (strcmp(method, "GET") == 0) return "GET";
    if (strcmp(method, "POST") == 0) return "POST";
    if (strcmp(method, "PUT") == 0) return "PUT";
    if (strcmp(method, "DELETE") == 0) return "DELETE";
    if (strcmp(method, "HEAD") == 0) return "HEAD";
    if (strcmp(method, "OPTION") == 0) return "OPTION";
    if (strcmp(method, "PATCH") == 0) return "PATCH";
    return "UNKNOWN METHOD";
}

const char* version_string_to_literal(const char* version) {
    if (!version) return "UNSUPPORTED VERSION";
    if (strcmp(version, "HTTP/1.1") == 0) return "HTTP/1.1";
    return "UNSUPPORTED VERSION";
}

Http_status_t parse_request_line(Http_request_t* req, Tcp_connection_t tcp_con) {
    char* line = NULL;
    while (!line) {
        line = get_line(tcp_con);
        if (!line) return PARSING_BROKEN_CLOSE_CONNECTION; //Unexpected EOF
        //RFC9112 2.2 empty line before request-line should be ignored
        if (strcmp(line, "\r\n") == 0 || strcmp(line, "\n") == 0) {
            free(line);
            line = NULL;
        }
    }
    char* method = strtok(line, " \r\n");
    char* target = strtok(NULL, " \r\n");
    char* version = strtok(NULL, " \r\n");
    char* leftover = strtok(NULL, " \r\n");
    req->request_line.method = method_string_to_literal(method);
    if (target) req->request_line.target = strdup(target);
    else req->request_line.target = strdup("");
    req->request_line.version = version_string_to_literal(version);
    //There should be only 3 parts
    if (leftover) {
        return HTTP_STATUS_BAD_REQUEST;
    }
    return PARSING_FINE;
}

Http_status_t parse_headers(Http_request_t* req, Tcp_connection_t tcp_con) {
    char* line = NULL;
    Http_status_t res = PARSING_FINE;
parse_next_field_line:
    line = get_line(tcp_con);
    if (!line) return PARSING_BROKEN_CLOSE_CONNECTION; //Unexpected EOF
    if (strcmp(line, "\r\n") == 0 || strcmp(line, "\n") == 0) return free(line), res; //End of headers
    //RFC9112 2.2 field lines starting with a white space shall be ignored
    if (isspace(line[0])) {
        free(line);
        goto parse_next_field_line;
    }
    //If there is no ":" it is invalid line
    char* ptr = line;
    int colon_found = 0;
    while (*ptr) {
        if (*ptr == ":") colon_found = 1;
    }
    if (!colon_found) {
        free(line);
        if (res == PARSING_FINE) res = HTTP_STATUS_BAD_REQUEST;
        goto parse_next_field_line;
    }
    const Field_line_t fl;
    fl.field_name = strtok(line, ":");
    fl.field_value = strtok(NULL, "\r\n");
    add_field_line_to_hash_map(&req->headers, fl);
    free(line);
    goto parse_next_field_line;
}

Http_status_t parse_body(Http_request_t* req, Tcp_connection_t tcp_con) {

}

Http_status_t parse_trailers(Http_request_t* req, Tcp_connection_t tcp_con) {

}

Http_status_t parse_http_request(Http_request_t* req, Tcp_connection_t tcp_con) {
    Http_status_t request_line_parsing_status = parse_request_line(req, tcp_con);
    if (request_line_parsing_status == PARSING_BROKEN_CLOSE_CONNECTION) {
        return PARSING_BROKEN_CLOSE_CONNECTION;
    }
    Http_status_t headers_parsing_status = parse_headers(req, tcp_con);
    if (headers_parsing_status == PARSING_BROKEN_CLOSE_CONNECTION) {
        return PARSING_BROKEN_CLOSE_CONNECTION;
    }
    Http_status_t body_parsing_status = parse_body(req, tcp_con);
    if (body_parsing_status == PARSING_BROKEN_CLOSE_CONNECTION) {
        return PARSING_BROKEN_CLOSE_CONNECTION;
    }
    Http_status_t trailers_parsing_status = parse_trailers(req, tcp_con);
    if (trailers_parsing_status == PARSING_BROKEN_CLOSE_CONNECTION) {
        return PARSING_BROKEN_CLOSE_CONNECTION;
    }
    //Return first encountered non-critical error or PARSING_FINE if no error occured
    if (request_line_parsing_status != PARSING_FINE) return request_line_parsing_status;
    if (headers_parsing_status != PARSING_FINE) return headers_parsing_status;
    if (body_parsing_status != PARSING_FINE) return body_parsing_status;
    if (trailers_parsing_status != PARSING_FINE) return trailers_parsing_status;
    return PARSING_FINE;
}

