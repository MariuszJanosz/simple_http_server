#include "http_response.h"
#include "http_routing.h"
#include "log.h"
#include "http_resources.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/sendfile.h>

extern char g_www_root[PATH_MAX];

void init_response(Http_response_t* res) {
    res->status_line = NULL;
    res->headers = NULL;
    res->resource_index = -1;
    res->trailers = NULL;
    res->has_body = 0;
    res->has_headers_hm = 0;
    res->has_trailers_hm = 0;
    res->send_chunked = 0;
    res->should_close = 0;
}

void free_body(Http_response_body_t* body);

void free_response(Http_response_t* res) {
    if (res->has_body) {
        free_body(&res->body);
    }
    if (res->has_headers_hm) {
        free(res->headers);
        free_field_line_hash_map(&res->headers_hm);
    }
    if (res->has_trailers_hm) {
        free(res->trailers);
        free_field_line_hash_map(&res->trailers_hm);
    }
}

void init_body(Http_response_body_t* body) {
#define INITIAL_BODY_CAPACITY 4
    body->capacity = INITIAL_BODY_CAPACITY;
    body->count = 0;
    body->size = 0;
    body->sections = malloc(body->capacity *
                            sizeof(*body->sections));
    if (!body->sections) {
        LOG(ERROR, "malloc failed!");
        exit(1);
    }
    body->section_types = malloc(   body->capacity *
                                    sizeof(*body->section_types));
    if (!body->section_types) {
        LOG(ERROR, "malloc failed!");
        exit(1);
    }
}

void free_body(Http_response_body_t* body) {
    for (size_t i = 0; i < body->count; ++i) {
        switch (body->section_types[i]) {
            case FILE_DESCRIPTOR:
                if (close(body->sections[i].fd_section.fd) < 0) {
                    LOG(ERROR, "close failed!");
                    exit(1);
                }
                break;
            case CHAR_BUFFER:
                free(body->sections[i].char_buff_section.buffer);
                break;
        }
    }
    free(body->sections);
    free(body->section_types);
}

size_t get_file_size(int fd) {
    struct stat st;
    if (fstat(fd, &st) < 0) {
        LOG(ERROR, "fstat failed!");
        exit(1);
    }
    return (size_t)st.st_size;
}

size_t get_headers_size(Field_line_hash_map_t* hm) {
    size_t res = 0;
    for (size_t i = 0; i < hm->capacity; ++i) {
        Field_line_hash_map_bucket_t* b = &hm->buckets[i];
        if (b->bucket_status != OCCUPIED) continue;
        for (size_t j = 0; j < b->field_line.count; ++j) {
            //field-name ":" field-value CRLF
            res +=  strlen(b->field_line.field_name)        + 1 +
                    strlen(b->field_line.field_values[j])   + 2;
        }
    }
    res += 2; //CRLF ending headers section
    return res;
}

char* field_line_hash_map_to_headers_string(Field_line_hash_map_t* hm) {
    size_t size = get_headers_size(hm);
    char* res = malloc((size + 1) * sizeof(*res));
    if (!res) {
        LOG(ERROR, "malloc failed!");
        exit(1);
    }
    size_t first_empty = 0;
    for (size_t i = 0; i < hm->capacity; ++i) {
        Field_line_hash_map_bucket_t* b = &hm->buckets[i];
        if (b->bucket_status != OCCUPIED) continue;
        for (size_t j = 0; j < b->field_line.count; ++j) {
            //field-name ":" field-value CRLF
            int n = sprintf(res + first_empty,
                            "%s:%s\r\n",
                            b->field_line.field_name,
                            b->field_line.field_values[j]);
            if (n < 0) {
                LOG(ERROR, "sprintf failed!");
                exit(1);
            }
            first_empty += n;
        }
    }
    sprintf(res + first_empty, "\r\n");
    return res;
}

void load_resource_to_body( Http_response_body_t* body,
                            char* resource_path) {
    size_t len = strlen(resource_path);
    const char* ext = ".resource";
    size_t ext_len = strlen(ext);
    if (len >= ext_len && strcmp(ext, &resource_path[len - ext_len]) == 0) {
        char file_path[PATH_MAX];
        size_t root_len = strlen(g_www_root);
        strcpy(file_path, g_www_root);
        int empty_line_found = 0;
        FILE* f = fopen(resource_path, "r");
        if (!f) {
            LOG(ERROR, "fopen failed!");
            exit(1);
        }
        size_t path_len = root_len;
        while (1) {
            int c = fgetc(f);
            if (c < 0) {
                if (feof(f) && empty_line_found) {
                    return; //OK
                }
                LOG(ERROR, "fgetc failed!");
                exit(1);
            }
            switch (c) {
                case '\n':
                    {
                        if (path_len == root_len) {
                            if (empty_line_found) {
                                LOG(ERROR, "Multiple empty lines in %s", resource_path);
                                exit(1);
                            }
                            empty_line_found = 1;
                        }
                        else {
                            if (path_len > PATH_MAX) {
                                LOG(ERROR, "One of the paths in %s is too long!", resource_path);
                                exit(1);
                            }
                            file_path[path_len] = '\0';
                            load_resource_to_body(body, file_path);
                            path_len = root_len;
                        }
                    }
                    break;
                default:
                    {
                        if (path_len > PATH_MAX) {
                            LOG(ERROR, "One of the paths in %s is too long!", resource_path);
                            exit(1);
                        }
                        file_path[path_len++] = c;
                    }
                    break;
            }
        }
    }
    else {
        //If it is not .resource file we simply append it to body as is
        int fd = open(resource_path, O_RDONLY);
        if (fd < 0) {
            LOG(ERROR, "open failed!");
            exit(1);
        }
        size_t size = get_file_size(fd);
        if (body->count == body->capacity) {
            body->capacity *= 2;
            Http_any_body_section_type_t* tmp =
                realloc(body->sections, body->capacity * sizeof(*tmp));
            if (!tmp) {
                LOG(ERROR, "realloc failed!");
                exit(1);
            }
            body->sections = tmp;
            Http_body_section_type_t* tmpp =
                realloc(body->section_types, body->capacity * sizeof(*tmpp));
            if (!tmpp) {
                LOG(ERROR, "realloc failed!");
                exit(1);
            }
            body->section_types = tmpp;
        }
        body->section_types[body->count] = FILE_DESCRIPTOR;
        body->sections[body->count].fd_section.fd = fd;
        body->sections[body->count].fd_section.size = size;
        body->count += 1;
        body->size += size;
    }
}

const char* get_status_line(Http_status_t status) {
#define VERSION "HTTP/1.1"
#define SP " "
#define CRLF "\r\n"
    switch (status) {
        case HTTP_STATUS_CONTINUE:
            return VERSION SP stringify(HTTP_STATUS_CONTINUE) SP CRLF;
        case HTTP_STATUS_SWITCHING_PROTOCOLS:
            return VERSION SP stringify(HTTP_STATUS_SWITCHING_PROTOCOLS) SP CRLF;
        case HTTP_STATUS_PROCESSING:
            return VERSION SP stringify(HTTP_STATUS_PROCESSING) SP CRLF;
        case HTTP_STATUS_EARLY_HINTS:
            return VERSION SP stringify(HTTP_STATUS_EARLY_HINTS) SP CRLF;
        case HTTP_STATUS_OK:
            return VERSION SP stringify(HTTP_STATUS_OK) SP CRLF;
        case HTTP_STATUS_CREATED:
            return VERSION SP stringify(HTTP_STATUS_CREATED) SP CRLF;
        case HTTP_STATUS_ACCEPTED:
            return VERSION SP stringify(HTTP_STATUS_ACCEPTED) SP CRLF;
        case HTTP_STATUS_NON_AUTHORITATIVE_INFORMATION:
            return VERSION SP stringify(HTTP_STATUS_NON_AUTHORITATIVE_INFORMATION) SP CRLF;
        case HTTP_STATUS_NO_CONTENT:
            return VERSION SP stringify(HTTP_STATUS_NO_CONTENT) SP CRLF;
        case HTTP_STATUS_RESET_CONTENT:
            return VERSION SP stringify(HTTP_STATUS_RESET_CONTENT) SP CRLF;
        case HTTP_STATUS_PARTIAL_CONTENT:
            return VERSION SP stringify(HTTP_STATUS_PARTIAL_CONTENT) SP CRLF;
        case HTTP_STATUS_MULTI_STATUS:
            return VERSION SP stringify(HTTP_STATUS_MULTI_STATUS) SP CRLF;
        case HTTP_STATUS_ALREADY_REPORTED:
            return VERSION SP stringify(HTTP_STATUS_ALREADY_REPORTED) SP CRLF;
        case HTTP_STATUS_IM_USED:
            return VERSION SP stringify(HTTP_STATUS_IM_USED) SP CRLF;
        case HTTP_STATUS_MULTIPLE_CHOICES:
            return VERSION SP stringify(HTTP_STATUS_MULTIPLE_CHOICES) SP CRLF;
        case HTTP_STATUS_MOVED_PERMANENTLY:
            return VERSION SP stringify(HTTP_STATUS_MOVED_PERMANENTLY) SP CRLF;
        case HTTP_STATUS_FOUND:
            return VERSION SP stringify(HTTP_STATUS_FOUND) SP CRLF;
        case HTTP_STATUS_SEE_OTHER:
            return VERSION SP stringify(HTTP_STATUS_SEE_OTHER) SP CRLF;
        case HTTP_STATUS_NOT_MODIFIED:
            return VERSION SP stringify(HTTP_STATUS_NOT_MODIFIED) SP CRLF;
        case HTTP_STATUS_USE_PROXY:
            return VERSION SP stringify(HTTP_STATUS_USE_PROXY) SP CRLF;
        case HTTP_STATUS_UNUSED:
            return VERSION SP stringify(HTTP_STATUS_UNUSED) SP CRLF;
        case HTTP_STATUS_TEMPORARY_REDIRECT:
            return VERSION SP stringify(HTTP_STATUS_TEMPORARY_REDIRECT) SP CRLF;
        case HTTP_STATUS_PERMANENT_REDIRECT:
            return VERSION SP stringify(HTTP_STATUS_PERMANENT_REDIRECT) SP CRLF;
        case HTTP_STATUS_BAD_REQUEST:
            return VERSION SP stringify(HTTP_STATUS_BAD_REQUEST) SP CRLF;
        case HTTP_STATUS_UNAUTHORIZED:
            return VERSION SP stringify(HTTP_STATUS_UNAUTHORIZED) SP CRLF;
        case HTTP_STATUS_PAYMENT_REQUIRED:
            return VERSION SP stringify(HTTP_STATUS_PAYMENT_REQUIRED) SP CRLF;
        case HTTP_STATUS_FORBIDDEN:
            return VERSION SP stringify(HTTP_STATUS_FORBIDDEN) SP CRLF;
        case HTTP_STATUS_NOT_FOUND:
            return VERSION SP stringify(HTTP_STATUS_NOT_FOUND) SP CRLF;
        case HTTP_STATUS_METHOD_NOT_ALLOWED:
            return VERSION SP stringify(HTTP_STATUS_METHOD_NOT_ALLOWED) SP CRLF;
        case HTTP_STATUS_NOT_ACCEPTABLE:
            return VERSION SP stringify(HTTP_STATUS_NOT_ACCEPTABLE) SP CRLF;
        case HTTP_STATUS_PROXY_AUTHENTICATION_REQUIRED:
            return VERSION SP stringify(HTTP_STATUS_PROXY_AUTHENTICATION_REQUIRED) SP CRLF;
        case HTTP_STATUS_REQUEST_TIMEOUT:
            return VERSION SP stringify(HTTP_STATUS_REQUEST_TIMEOUT) SP CRLF;
        case HTTP_STATUS_CONFLICT:
            return VERSION SP stringify(HTTP_STATUS_CONFLICT) SP CRLF;
        case HTTP_STATUS_GONE:
            return VERSION SP stringify(HTTP_STATUS_GONE) SP CRLF;
        case HTTP_STATUS_LENGTH_REQUIRED:
            return VERSION SP stringify(HTTP_STATUS_LENGTH_REQUIRED) SP CRLF;
        case HTTP_STATUS_PRECONDITION_FAILED:
            return VERSION SP stringify(HTTP_STATUS_PRECONDITION_FAILED) SP CRLF;
        case HTTP_STATUS_PAYLOAD_TOO_LARGE:
            return VERSION SP stringify(HTTP_STATUS_PAYLOAD_TOO_LARGE) SP CRLF;
        case HTTP_STATUS_URI_TOO_LONG:
            return VERSION SP stringify(HTTP_STATUS_URI_TOO_LONG) SP CRLF;
        case HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE:
            return VERSION SP stringify(HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE) SP CRLF;
        case HTTP_STATUS_RANGE_NOT_SATISFIABLE:
            return VERSION SP stringify(HTTP_STATUS_RANGE_NOT_SATISFIABLE) SP CRLF;
        case HTTP_STATUS_EXPECTATION_FAILED:
            return VERSION SP stringify(HTTP_STATUS_EXPECTATION_FAILED) SP CRLF;
        case HTTP_STATUS_I_M_A_TEAPOT:
            return VERSION SP stringify(HTTP_STATUS_I_M_A_TEAPOT) SP CRLF;
        case HTTP_STATUS_MISDIRECTED_REQUEST:
            return VERSION SP stringify(HTTP_STATUS_MISDIRECTED_REQUEST) SP CRLF;
        case HTTP_STATUS_UNPROCESSABLE_ENTITY:
            return VERSION SP stringify(HTTP_STATUS_UNPROCESSABLE_ENTITY) SP CRLF;
        case HTTP_STATUS_LOCKED:
            return VERSION SP stringify(HTTP_STATUS_LOCKED) SP CRLF;
        case HTTP_STATUS_FAILED_DEPENDANCY:
            return VERSION SP stringify(HTTP_STATUS_FAILED_DEPENDANCY) SP CRLF;
        case HTTP_STATUS_TOO_EARLY:
            return VERSION SP stringify(HTTP_STATUS_TOO_EARLY) SP CRLF;
        case HTTP_STATUS_UPGRADE_REQUIRED:
            return VERSION SP stringify(HTTP_STATUS_UPGRADE_REQUIRED) SP CRLF;
        case HTTP_STATUS_PRECONDITION_REQUIRED:
            return VERSION SP stringify(HTTP_STATUS_PRECONDITION_REQUIRED) SP CRLF;
        case HTTP_STATUS_TOO_MANY_REQUESTS:
            return VERSION SP stringify(HTTP_STATUS_TOO_MANY_REQUESTS) SP CRLF;
        case HTTP_STATUS_REQUEST_HEADER_FIELDS_TOO_LARGE:
            return VERSION SP stringify(HTTP_STATUS_REQUEST_HEADER_FIELDS_TOO_LARGE) SP CRLF;
        case HTTP_STATUS_UNAVAILABLE_FOR_LEGAL_REASONS:
            return VERSION SP stringify(HTTP_STATUS_UNAVAILABLE_FOR_LEGAL_REASONS) SP CRLF;
        case HTTP_STATUS_INTERNAL_SERVER_ERROR:
            return VERSION SP stringify(HTTP_STATUS_INTERNAL_SERVER_ERROR) SP CRLF;
        case HTTP_STATUS_NOT_IMPLEMENTED:
            return VERSION SP stringify(HTTP_STATUS_NOT_IMPLEMENTED) SP CRLF;
        case HTTP_STATUS_BAD_GATEWAY:
            return VERSION SP stringify(HTTP_STATUS_BAD_GATEWAY) SP CRLF;
        case HTTP_STATUS_SERVICE_UNAVAILABLE:
            return VERSION SP stringify(HTTP_STATUS_SERVICE_UNAVAILABLE) SP CRLF;
        case HTTP_STATUS_GATEWAY_TIMEOUT:
            return VERSION SP stringify(HTTP_STATUS_GATEWAY_TIMEOUT) SP CRLF;
        case HTTP_STATUS_HTTP_VERSION_NOT_SUPPORTED:
            return VERSION SP stringify(HTTP_STATUS_HTTP_VERSION_NOT_SUPPORTED) SP CRLF;
        case HTTP_STATUS_VARIANT_ALSO_NEGOTIATES:
            return VERSION SP stringify(HTTP_STATUS_VARIANT_ALSO_NEGOTIATES) SP CRLF;
        case HTTP_STATUS_INSUFFICIENT_STORAGE:
            return VERSION SP stringify(HTTP_STATUS_INSUFFICIENT_STORAGE) SP CRLF;
        case HTTP_STATUS_LOOP_DETECTED:
            return VERSION SP stringify(HTTP_STATUS_LOOP_DETECTED) SP CRLF;
        case HTTP_STATUS_NOT_EXTENDED:
            return VERSION SP stringify(HTTP_STATUS_NOT_EXTENDED) SP CRLF;
        case HTTP_STATUS_NETWORK_AUTHENTICATION_REQUIRED:
            return VERSION SP stringify(HTTP_STATUS_NETWORK_AUTHENTICATION_REQUIRED) SP CRLF;
        default:
            return VERSION SP "500" SP CRLF;
    }
#undef VERSION
#undef SP
#undef CRLF
}

int has_chunked_header(Http_response_t* res) {
    Field_line_t* trencfl = find_field_line_in_hash_map(&res->headers_hm, "Transfer-Encoding");
    if (!trencfl) return 0;
    for (size_t i = 0; i < trencfl->count; ++i) {
        char* str = trencfl->field_values[i];
        char* p = strstr(str, "chunked");
        if (p && i < trencfl->count - 1) return 0;
        if (i == trencfl->count - 1) {
            if (!p) return 0;
            if (p + strlen("chunked") != str + strlen(str)) return 0;
            return 1;
        }
    }
    return 0;
}

Http_status_t prepare_response_for_error(   Http_response_t* res,
                                            Http_request_context_t* req_con);

Http_status_t get_handler(  Http_response_t* res,
                            Http_request_context_t* req_con) {
    route_http_request(req_con, res);
    if (res->resource_index == -1) {
        req_con->status = HTTP_STATUS_NOT_FOUND;
        return prepare_response_for_error(res, req_con);
    }
    Resource_t resource = g_resources_array[res->resource_index];
    res->status_line = get_status_line(resource.status);
    init_body(&res->body);
    load_resource_to_body(&res->body, resource.target);
    res->has_body = 1;
    init_field_line_hash_map(&res->headers_hm, 2 * resource.count + 4);
    res->has_headers_hm = 1;
    for (size_t i = 0; i < resource.count; ++i) {
        Field_line_config_t flconf = resource.field_line_configs[i];
        if (flconf.field_value) {
            char* s1 = strdup(flconf.field_name);
            char* s2 = strdup(flconf.field_value);
            if (s1 && s2) {
                add_field_line_to_hash_map(&res->headers_hm, s1, s2);
            }
            else {
                LOG(ERROR, "strdup failed!");
                exit(1);
            }
        }
        else {
            if (strcmp(flconf.func, "file_size") == 0) {
                char size[128];
                sprintf(size, "%zu", res->body.size);
                char* s1 = strdup(flconf.field_name);
                char* s2 = strdup(size);
                if (s1 && s2) {
                    add_field_line_to_hash_map(&res->headers_hm, s1, s2);
                }
                else {
                    LOG(ERROR, "strdup failed!");
                    exit(1);
                }
            }
            else {
                LOG(ERROR, "unknown func!");
                exit(1);
            }
        }
    }
    if (res->should_close) {
        char* s1 = strdup("Connection");
        char* s2 = strdup("close");
        add_field_line_to_hash_map(&res->headers_hm, s1, s2);
    }
    res->headers = field_line_hash_map_to_headers_string(&res->headers_hm);
    if (has_chunked_header(res)) {
        res->send_chunked = 1;
        res->trailers = "\r\n";
    }
    return resource.status;
}

Http_status_t default_handler(  Http_response_t* res,
                                Http_request_context_t* req_con) {
    req_con->status = HTTP_STATUS_NOT_IMPLEMENTED;
    return prepare_response_for_error(res, req_con);
}

Http_status_t prepare_response_for_valid_req_con(Http_response_t* res,
                                      Http_request_context_t* req_con) {
    //For now only GET is supported
    if (strcmp("GET", req_con->req.request_line.method) == 0) {
        return get_handler(res, req_con);
    }
    else {
        return default_handler(res, req_con);
    }
}

Http_status_t prepare_response_for_error(   Http_response_t* res,
                                            Http_request_context_t* req_con) {
#define VERSION "HTTP/1.1"
#define SP " "
#define CRLF "\r\n"
#define case_GENERIC_RESPONSE(status_macro) case status_macro:\
res->status_line = VERSION SP stringify(status_macro) SP CRLF;\
res->headers = CRLF;\
break;
    switch (req_con->status) {
        case PARSING_BROKEN_CLOSE_CONNECTION:
            res->status_line = VERSION SP stringify(HTTP_STATUS_BAD_REQUEST) SP CRLF;
            res->headers = "Connection:close" CRLF CRLF;
            break;
        case_GENERIC_RESPONSE(HTTP_STATUS_BAD_REQUEST)
        case_GENERIC_RESPONSE(HTTP_STATUS_UNAUTHORIZED)
        case_GENERIC_RESPONSE(HTTP_STATUS_PAYMENT_REQUIRED)
        case_GENERIC_RESPONSE(HTTP_STATUS_FORBIDDEN)
        case_GENERIC_RESPONSE(HTTP_STATUS_NOT_FOUND)
        case_GENERIC_RESPONSE(HTTP_STATUS_METHOD_NOT_ALLOWED)
        case_GENERIC_RESPONSE(HTTP_STATUS_NOT_ACCEPTABLE)
        case_GENERIC_RESPONSE(HTTP_STATUS_PROXY_AUTHENTICATION_REQUIRED)
        case_GENERIC_RESPONSE(HTTP_STATUS_REQUEST_TIMEOUT)
        case_GENERIC_RESPONSE(HTTP_STATUS_CONFLICT)
        case_GENERIC_RESPONSE(HTTP_STATUS_GONE)
        case_GENERIC_RESPONSE(HTTP_STATUS_LENGTH_REQUIRED)
        case_GENERIC_RESPONSE(HTTP_STATUS_PRECONDITION_FAILED)
        case_GENERIC_RESPONSE(HTTP_STATUS_PAYLOAD_TOO_LARGE)
        case_GENERIC_RESPONSE(HTTP_STATUS_URI_TOO_LONG)
        case_GENERIC_RESPONSE(HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE)
        case_GENERIC_RESPONSE(HTTP_STATUS_RANGE_NOT_SATISFIABLE)
        case_GENERIC_RESPONSE(HTTP_STATUS_EXPECTATION_FAILED)
        case_GENERIC_RESPONSE(HTTP_STATUS_I_M_A_TEAPOT)
        case_GENERIC_RESPONSE(HTTP_STATUS_MISDIRECTED_REQUEST)
        case_GENERIC_RESPONSE(HTTP_STATUS_UNPROCESSABLE_ENTITY)
        case_GENERIC_RESPONSE(HTTP_STATUS_LOCKED)
        case_GENERIC_RESPONSE(HTTP_STATUS_FAILED_DEPENDANCY)
        case_GENERIC_RESPONSE(HTTP_STATUS_TOO_EARLY)
        case_GENERIC_RESPONSE(HTTP_STATUS_UPGRADE_REQUIRED)
        case_GENERIC_RESPONSE(HTTP_STATUS_PRECONDITION_REQUIRED)
        case_GENERIC_RESPONSE(HTTP_STATUS_TOO_MANY_REQUESTS)
        case_GENERIC_RESPONSE(HTTP_STATUS_REQUEST_HEADER_FIELDS_TOO_LARGE)
        case_GENERIC_RESPONSE(HTTP_STATUS_UNAVAILABLE_FOR_LEGAL_REASONS)
        case_GENERIC_RESPONSE(HTTP_STATUS_INTERNAL_SERVER_ERROR)
        case_GENERIC_RESPONSE(HTTP_STATUS_NOT_IMPLEMENTED)
        case_GENERIC_RESPONSE(HTTP_STATUS_BAD_GATEWAY)
        case_GENERIC_RESPONSE(HTTP_STATUS_SERVICE_UNAVAILABLE)
        case_GENERIC_RESPONSE(HTTP_STATUS_GATEWAY_TIMEOUT)
        case_GENERIC_RESPONSE(HTTP_STATUS_HTTP_VERSION_NOT_SUPPORTED)
        case_GENERIC_RESPONSE(HTTP_STATUS_VARIANT_ALSO_NEGOTIATES)
        case_GENERIC_RESPONSE(HTTP_STATUS_INSUFFICIENT_STORAGE)
        case_GENERIC_RESPONSE(HTTP_STATUS_LOOP_DETECTED)
        case_GENERIC_RESPONSE(HTTP_STATUS_NOT_EXTENDED)
        case_GENERIC_RESPONSE(HTTP_STATUS_NETWORK_AUTHENTICATION_REQUIRED)
    }
    return req_con->status;
#undef VERSION
#undef SP
#undef CRLF
#undef case_GENERIC_RESPONSE
}

Http_status_t prepare_response( Http_response_t* res,
                                Http_request_context_t* req_con) {
    if (req_con->close_connection_after_response) {
        res->should_close = 1;
    }
    if (req_con->status == REQUEST_PROCESSING_FINE)
        return prepare_response_for_valid_req_con(res, req_con);

    return prepare_response_for_error(res, req_con);
}

void send_buf(const int fd, const char* buf, const size_t size) {
    size_t n = 0;
    while (n < size) {
        ssize_t tmp = write(fd, buf + n, size - n);
        if (tmp < 0) {
            LOG(ERROR, "write failed!");
            exit(1);
        }
        n += tmp;
    }
}

void read_buf(const int fd, char* buf, const size_t size) {
    size_t n = 0;
    while (n < size) {
        ssize_t r = read(fd, buf + n, size - n);
        if (r < 0) {
            LOG(ERROR, "read failed!");
            exit(1);
        }
        n += r;
    }
}

size_t get_chunk_size_str(char* out, size_t in) {
    if (in == 0) {
        out[0] = '0';
        out[1] = '\r';
        out[2] = '\n';
        return 3;
    }
    size_t len = 0;
    while (in) {
        const char conv_tb[] = {'0','1','2','3','4','5','6','7','8','9',
                                'A','B','C','D','E','F'};
        int digit = in % 16;
        in /= 16;
        out[len] = conv_tb[digit];
        ++len;
    }
    out[len] = '\r';
    ++len;
    out[len] = '\n';
    ++len;
    for (int i = 0; i < (len - 2) / 2; ++i) {
        char tmp = out[i];
        out[i] = out[len - 3 - i];
        out[len - 3 - i] = tmp;
    }
    return len;
}

#define MAX_CHUNK_SIZE (16 * 1024) //16kB

void send_body_section_FILE_DESCRIPTOR_chunked(
        Http_body_section_FILE_DESCRIPTOR_t* fd_sec,
        Tcp_connection_t tcp_con) {
    size_t sent_bytes = 0;
    while (sent_bytes < fd_sec->size) {
        size_t bytes_to_send =  (fd_sec->size - sent_bytes > MAX_CHUNK_SIZE)?
                                MAX_CHUNK_SIZE:
                                fd_sec->size - sent_bytes;
        char chunk[MAX_CHUNK_SIZE];
        read_buf(fd_sec->fd, chunk, bytes_to_send);
        char chunk_size[16];
        size_t chunk_size_len = get_chunk_size_str(chunk_size, bytes_to_send);
        send_buf(tcp_con.fd, chunk_size, chunk_size_len);
        send_buf(tcp_con.fd, chunk, bytes_to_send);
        send_buf(tcp_con.fd, "\r\n", 2);
        sent_bytes += bytes_to_send;
    }
}

void send_body_section_CHAR_BUFFER_chunked(
        Http_body_section_CHAR_BUFFER_t* char_buff_sec,
        Tcp_connection_t tcp_con) {
    size_t sent_bytes = 0;
    while (sent_bytes < char_buff_sec->size) {
        size_t bytes_to_send =  (char_buff_sec->size - sent_bytes > MAX_CHUNK_SIZE)?
                                MAX_CHUNK_SIZE:
                                char_buff_sec->size - sent_bytes;
        char chunk_size[16];
        size_t chunk_size_len = get_chunk_size_str(chunk_size, bytes_to_send);
        send_buf(tcp_con.fd, chunk_size, chunk_size_len);
        send_buf(tcp_con.fd, char_buff_sec->buffer + sent_bytes, bytes_to_send);
        send_buf(tcp_con.fd, "\r\n", 2);
        sent_bytes += bytes_to_send;
    }
}

void send_body_chunked( Http_response_body_t* body,
                        Tcp_connection_t tcp_con) {
    for (size_t i = 0; i < body->count; ++i) {
        switch (body->section_types[i]) {
            case FILE_DESCRIPTOR:
                send_body_section_FILE_DESCRIPTOR_chunked(
                        &body->sections[i].fd_section,
                        tcp_con);
                break;
            case CHAR_BUFFER:
                send_body_section_CHAR_BUFFER_chunked(
                        &body->sections[i].char_buff_section,
                        tcp_con);
                break;
        }
    }
    send_buf(tcp_con.fd, "0\r\n", 3);
}

void send_body_cl(  Http_response_body_t* body,
                    Tcp_connection_t tcp_con) {
    for (size_t i = 0; i < body->count; ++i) {
        switch (body->section_types[i]) {
            case FILE_DESCRIPTOR:
                if (sendfile(   tcp_con.fd, body->sections[i].fd_section.fd,
                                NULL, body->sections[i].fd_section.size) < 0) {
                    LOG(ERROR, "sendfile failed!");
                    exit(1);
                }
                break;
            case CHAR_BUFFER:
                send_buf(   tcp_con.fd,
                            body->sections[i].char_buff_section.buffer,
                            body->sections[i].char_buff_section.size);
                break;
        }
    }
}

void send_response( Http_response_t* res,
                    Tcp_connection_t tcp_con) {
    send_buf(tcp_con.fd, res->status_line, strlen(res->status_line));
    send_buf(tcp_con.fd, res->headers, strlen(res->headers));
    if (res->has_body) {
        if (res->send_chunked) {
            send_body_chunked(&res->body, tcp_con);
            send_buf(tcp_con.fd, res->trailers, strlen(res->trailers));
        }
        else send_body_cl(&res->body, tcp_con);
    }
    if (res->should_close) {
        //If there are further piped requests, they will fail.
        //It is fine anyway, messages after close are invalid.
        close(tcp_con.fd);
    }
}

void print_body(Http_response_body_t* body) {
#define MAX_SIZE_RESPONSE_BODY_TO_LOG (32 * 1024)
    if (body->size <= MAX_SIZE_RESPONSE_BODY_TO_LOG) {
        for (size_t i = 0; i < body->count; ++i) {
            switch (body->section_types[i]) {
                case FILE_DESCRIPTOR:
                {
                    Http_body_section_FILE_DESCRIPTOR_t* fd_sec = &body->sections[i].fd_section;
                    //move fd offset back to the beginning of the file
                    if (lseek(fd_sec->fd, 0, SEEK_SET) < 0) {
                        LOG(ERROR, "lseek failed!");
                        exit(1);
                    }
                    //print in chunks
                    size_t sent_bytes = 0;
                    while (sent_bytes < fd_sec->size) {
                        size_t bytes_to_send = (fd_sec->size - sent_bytes > MAX_CHUNK_SIZE)?
                                                MAX_CHUNK_SIZE:
                                                fd_sec->size - sent_bytes;
                        char chunk[MAX_CHUNK_SIZE];
                        read_buf(fd_sec->fd, chunk, bytes_to_send);
                        send_buf(STDOUT_FILENO, chunk, bytes_to_send);
                        sent_bytes += bytes_to_send;
                    }
                } break;
                case CHAR_BUFFER:
                    printf("%.*s",
                        (int)body->sections[i].char_buff_section.size,
                        body->sections[i].char_buff_section.buffer);
                    break;
            }
        }
    }
    else {
        printf("Body too large to print!\n");
    }
}

void print_response(Http_response_t* res) {
    printf("%s", res->status_line);
    printf("%s", res->headers);
    if (res->has_body) {
        print_body(&res->body);
    }
    if (res->send_chunked) {
        printf("%s", res->trailers);
    }
}

