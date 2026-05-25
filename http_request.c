#include "http_request.h"
#include "http_field_line.h"
#include "reader.h"
#include "http_normalize_field_line.h"
#include "log.h"

#include <string.h>
#include <ctype.h>
#include <stdlib.h>

void init_http_request(Http_request_t* req) {
    req->request_line.method = NULL;
    req->request_line.target = NULL;
    req->request_line.version = NULL;
    init_field_line_hash_map(&req->headers, 32);
    req->body = NULL;
    req->body_size = 0;
    req->has_trailers_section = 0;
}

void free_http_request(Http_request_t* req) {
    req->request_line.method = NULL;
    if (req->request_line.target) free(req->request_line.target);
    req->request_line.target = NULL;
    req->request_line.version = NULL;
    free_field_line_hash_map(&req->headers);
    if (req->body) free(req->body);
    req->body = NULL;
    req->body_size = 0;
    if (req->has_trailers_section) {
        req->has_trailers_section = 0;
        free_field_line_hash_map(&req->trailers);
    }
}

void clean_http_request(Http_request_t* req) {
    req->request_line.method = NULL;
    if (req->request_line.target) free(req->request_line.target);
    req->request_line.target = NULL;
    req->request_line.version = NULL;
    clean_field_line_hash_map(&req->headers);
    if (req->body) free(req->body);
    req->body = NULL;
    req->body_size = 0;
    if (req->has_trailers_section) {
        req->has_trailers_section = 0;
        free_field_line_hash_map(&req->trailers);
    }
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
    Http_status_t res = PARSING_FINE;
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
    else {
        res = HTTP_STATUS_BAD_REQUEST;
        req->request_line.target = strdup("");
    }
    req->request_line.version = version_string_to_literal(version);
    //There should be only 3 parts
    if (leftover) {
        res = HTTP_STATUS_BAD_REQUEST;
    }
    return res;
}

Http_status_t parse_field_lines(Field_line_hash_map_t* hm, Tcp_connection_t tcp_con) {
    Http_status_t res = PARSING_FINE;
    char* line = NULL;
parse_next_field_line:
    line = get_line(tcp_con);
    if (!line) return PARSING_BROKEN_CLOSE_CONNECTION; //Unexpected EOF
    if (strcmp(line, "\r\n") == 0 || strcmp(line, "\n") == 0) return free(line), res; //End of headers
    //RFC9112 2.2 field lines starting with a white space shall be ignored
    if (isspace((unsigned char)line[0])) {
        free(line);
        goto parse_next_field_line;
    }
    //If there is no ":" it is invalid line
    char* ptr = line;
    int colon_found = 0;
    while (*ptr) {
        if (*ptr == ':') colon_found = 1;
    }
    if (!colon_found) {
        free(line);
        if (res == PARSING_FINE) res = HTTP_STATUS_BAD_REQUEST;
        goto parse_next_field_line;
    }
    char* field_name = strtok(line, ":");
    char* field_value = strtok(NULL, "\r\n");
    if (!field_value) field_value = "";
    //RFC9112 5.1 whitespace between field-name and ":" not allowed
    if (isspace((unsigned char)field_name[strlen(field_name) - 1])) {
        free(line);
        if (res == PARSING_FINE) res = HTTP_STATUS_BAD_REQUEST;
        goto parse_next_field_line;
    }
    add_field_line_to_hash_map(hm, field_name, field_value);
    free(line);
    goto parse_next_field_line;
}

Http_status_t parse_headers(Http_request_t* req, Tcp_connection_t tcp_con) {
    return parse_field_lines(&req->headers, tcp_con);
}

Http_status_t parse_body_cl(Http_request_t* req, Tcp_connection_t tcp_con, size_t content_length) {
    req->body = malloc(content_length);
    if (!req->body) {
        LOG(ERROR, "malloc failed!");
        exit(1);
    }
    size_t read = 0;
    while (read < content_length) {
        size_t r = get_data(tcp_con, req->body + read, content_length - read);
        if (r == 0) {//EOF before entire content was transfered
            free(req->body);
            req->body = NULL;
            return PARSING_BROKEN_CLOSE_CONNECTION;
        }
        read += r;
    }
    req->body_size = content_length;
    return PARSING_FINE;
}

Http_status_t parse_body_chunked(Http_request_t* req, Tcp_connection_t tcp_con) {
    size_t body_capacity = 4 * 1024; //4kB
    req->body = malloc(body_capacity);
    if (!req->body) {
        LOG(ERROR, "malloc failed!");
        exit(1);
    }
    req->body_size = 0;
    while (1) {
        //Parse size line
        char* line = get_line(tcp_con);
        if (!line) {
            free(req->body);
            req->body = NULL;
            return PARSING_BROKEN_CLOSE_CONNECTION;
        }
        char* size_str = line;
        char* chunk_ext = line;
        char* CRLF;
        //Find the end of size_str
        while (*chunk_ext != ';' || *chunk_ext != '\r' ||
               *chunk_ext != '\n' || *chunk_ext != '\0')
            ++chunk_ext;
        if (*chunk_ext == ';') {
            chunk_ext = '\0';
            ++chunk_ext;
            //Find the end of chunk_ext
            CRLF = chunk_ext;
            while (*CRLF != '\r' || *CRLF != '\n' || *CRLF != '\0') ++CRLF;
        }
        else {
            CRLF = chunk_ext;
            chunk_ext = NULL;
        }
        //If line contain '\r' in the middle
        if (!(*CRLF == '\r' && *(CRLF + 1) == '\n') && !(*CRLF == '\n')) {
            free(line), free(req->body);
            req->body = NULL;
            return PARSING_BROKEN_CLOSE_CONNECTION;
        }
        *CRLF = '\0';
        //TODO: for now chunk_ext is not supported
        if (chunk_ext) {
            free(line), free(req->body);
            req->body = NULL;
            return PARSING_BROKEN_CLOSE_CONNECTION;
        }
        //Get_chunk_size
        size_t chunk_size = 0;
        while (*size_str) {
            if ('0' <= *size_str && *size_str <= '9') {
                chunk_size *= 16;
                chunk_size += (*size_str - '0');
            }
            else if ('a' <= tolower((unsigned char)*size_str) &&
                            tolower((unsigned char)*size_str) <= 'f') {
                chunk_size *= 16;
                chunk_size += (tolower((unsigned char)*size_str) - 'a') + 10;
            }
            //Invalid chunk_size
            else {
                free(line), free(req->body);
                req->body = NULL;
                return PARSING_BROKEN_CLOSE_CONNECTION;
            }
            if (chunk_size > MAX_REQUEST_CHUNK_SIZE) {
                free(line), free(req->body);
                req->body = NULL;
                return PARSING_BROKEN_CLOSE_CONNECTION;
            }
            ++size_str;
        }
        //line is no longer needed
        free(line);
        //if chunk_size is 0 it is last-chunk
        if (chunk_size == 0) {
            break;
        }
        //else read chunk
        if (req->body_size + chunk_size > MAX_REQUEST_BODY_SIZE) {
            free(req->body);
            req->body = NULL;
            return PARSING_BROKEN_CLOSE_CONNECTION;
        }
        size_t read = 0;
        static char chunk[MAX_REQUEST_CHUNK_SIZE];
        while (read < chunk_size) {
            size_t r = get_data(tcp_con, chunk + read, chunk_size - read);
            if (r == 0) {//EOF before entire content was transfered
                free(req->body);
                req->body = NULL;
                return PARSING_BROKEN_CLOSE_CONNECTION;
            }
            read += r;
        }
        //coyp chunk to req->body, realloc if needed
        if (req->body_size + chunk_size > body_capacity) {
            body_capacity *= 2;
            char* tmp = realloc(req->body, body_capacity);
            if (!tmp) {
                LOG(ERROR, "realloc failed!");
                exit(1);
            }
            req->body = tmp;
        }
        memcpy(req->body + req->body_size, chunk, chunk_size);
        req->body += chunk_size;
        char* trailingCRLF = get_line(tcp_con);
        if (!trailingCRLF) {
            free(trailingCRLF), free(req->body);
            req->body = NULL;
            return PARSING_BROKEN_CLOSE_CONNECTION;
        }
        if (strcmp(trailingCRLF, "\n") == 0 || strcmp(trailingCRLF, "\r\n") == 0) {//OK
            free(trailingCRLF);
        }
        else {
            free(trailingCRLF), free(req->body);
            req->body = NULL;
            return PARSING_BROKEN_CLOSE_CONNECTION;
        }
    }
    return PARSING_FINE;
}

Http_status_t parse_body(Http_request_t* req, Tcp_connection_t tcp_con) {
    //Check if req has chunk encoded body
    Field_line_t* transfer_encoding_fl = find_field_line_in_hash_map(&req->headers, "Transfer-Encoding");
    if (transfer_encoding_fl) {
        const char* chunked_str = "chunked";
        size_t len = strlen(chunked_str);
        char* val = transfer_encoding_fl->field_values[0];
        int pos = 0;
        while (val[pos]) {
            int j = 0;
            for (; j < len; ++j) {
                if (val[pos + j] != chunked_str[j]) break;
            }
            if (j != len) {
                pos += j;
                while (val[pos] != ',' && val[pos] != '\0') ++pos;
                if (val[pos] == ',') ++pos;
            }
            else if (val[pos + j] == '\0') {
                init_field_line_hash_map(&req->trailers, 8);
                req->has_trailers_section = 1;
                return parse_body_chunked(req, tcp_con);
            }
            else if (val[pos + j] == ',') {
                return PARSING_BROKEN_CLOSE_CONNECTION;
            }
            else {
                pos += j;
                while (val[pos] != ',' && val[pos] != '\0') ++pos;
                if (val[pos] == ',') ++pos;
            }
        }
    }
    //Check if req has body with a given length
    Field_line_t* content_length_fl = find_field_line_in_hash_map(&req->headers, "Content-Length");
    if (content_length_fl) {
        char* ptr = content_length_fl->field_values[0];
        size_t content_length = 0;
        do {
            if ('0' <= *ptr && *ptr <= 9) {
                content_length *= 10;
                content_length += (*ptr - '0');
                if (content_length > MAX_REQUEST_BODY_SIZE) {
                    return PARSING_BROKEN_CLOSE_CONNECTION;
                }
            }
            else {
                return PARSING_BROKEN_CLOSE_CONNECTION;
            }
            ++ptr;
        } while (*ptr != '\0');
        if (content_length) return parse_body_cl(req, tcp_con, content_length);
    }
    //If we are here there is no body
    return PARSING_FINE;
}

Http_status_t parse_trailers(Http_request_t* req, Tcp_connection_t tcp_con) {
    return parse_field_lines(&req->trailers, tcp_con);
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
    Http_status_t headers_normalization_status = normalize(&req->headers);
    if (headers_normalization_status == PARSING_BROKEN_CLOSE_CONNECTION) {
        return PARSING_BROKEN_CLOSE_CONNECTION;
    }
    Http_status_t body_parsing_status = parse_body(req, tcp_con);
    if (body_parsing_status == PARSING_BROKEN_CLOSE_CONNECTION) {
        return PARSING_BROKEN_CLOSE_CONNECTION;
    }
    Http_status_t trailers_parsing_status = PARSING_FINE;
    Http_status_t trailers_normalization_status = PARSING_FINE;
    if (req->has_trailers_section) {
        trailers_parsing_status = parse_trailers(req, tcp_con);
        if (trailers_parsing_status == PARSING_BROKEN_CLOSE_CONNECTION) {
            return PARSING_BROKEN_CLOSE_CONNECTION;
        }
        trailers_normalization_status = normalize(&req->trailers);
        if (trailers_normalization_status == PARSING_BROKEN_CLOSE_CONNECTION) {
            return PARSING_BROKEN_CLOSE_CONNECTION;
        }
    }
    //Return first encountered non-critical error or PARSING_FINE if no error occured
    if (request_line_parsing_status != PARSING_FINE) return request_line_parsing_status;
    if (headers_parsing_status != PARSING_FINE) return headers_parsing_status;
    if (headers_normalization_status != PARSING_FINE) return headers_normalization_status;
    if (body_parsing_status != PARSING_FINE) return body_parsing_status;
    if (trailers_parsing_status != PARSING_FINE) return trailers_parsing_status;
    if (trailers_normalization_status != PARSING_FINE) return trailers_normalization_status;
    return PARSING_FINE;
}

