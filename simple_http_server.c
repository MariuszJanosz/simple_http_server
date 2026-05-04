#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <threads.h>
#include <signal.h>
#include <stdint.h>
#include <limits.h>

#include <unistd.h>

#include "log.h"
#include "reader.h"
#include "tcp_connection.h"
#include "http_message.h"
#include "http_routing.h"

#define IP (((unsigned int)127*(1<<24))+(0*(1<<16))+(0*(1<<8))+1) //127.0.0.1 localhost
#define PORT 54321

typedef struct Http_server_t {
    int tcp_listener_fd;
    Tcp_connection_t tcp_connection;
    Input_queue_t* input_queue;
} Http_server_t;

void echo_request(Http_message_t* req) {
    static int req_id = 0;
    Request_line_t* req_line = (Request_line_t*)req->start_line;
    printf("---req---echo---id:%05d---\n", req_id++);
    printf("%s %s %s\n",
        http_method_to_string(req_line->method),
        req_line->request_target,
        req_line->http_version);
    for (int i = 0; i < req->field_lines_count; ++i) {
        printf("%s: %s\n", req->field_lines[i].field_name, req->field_lines[i].field_value);
    }
    printf("\n");
    if (req->message_body) {
        if (req->body_size <= INT_MAX) {
            printf("%.*s\n", (int)req->body_size, req->message_body);
        }
        else {
            LOG(INFO, "Body too big, printf skipped!");
        }
        printf("\n");
    }
}

void echo_response(Http_message_t* res) {
    static int res_id = 0;
    Status_line_t* status_line = (Status_line_t*)res->start_line;
    printf("---res---echo---id:%05d---\n", res_id++);
    printf("%s %s %s\n",
        status_line->http_version,
        status_line->status_code,
        status_line->status_text);
    for (int i = 0; i < res->field_lines_count; ++i) {
        printf("%s: %s\n", res->field_lines[i].field_name, res->field_lines[i].field_value);
    }
    printf("\n");
    if (res->message_body) {
        if (res->body_size <= INT_MAX) {
            printf("%.*s\n", (int)res->body_size, res->message_body);
        }
        else {
            LOG(INFO, "Body too big, printf skipped!");
        }
        printf("\n");
    }
}

long get_file_size(FILE* f) {
    long curr = ftell(f);
    if (curr < 0) {
        LOG(ERROR, "ftell failed!");
    }
    if (fseek(f, 0, SEEK_END)) {
        LOG(ERROR, "fseek failed!");
        exit(1);
    }
    long res = ftell(f);
    if (res < 0) {
        LOG(ERROR, "ftell failed!");
        exit(1);
    }
    if (fseek(f, curr, SEEK_SET)) {
        LOG(ERROR, "fseek failed!");
        exit(1);
    }
    return res;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("USAGE: simple_http_server <path_to_www_root>\n");
        return 1;
    }
    
    char www_root[PATH_MAX];
    if (!realpath(argv[1], www_root)) {
        printf("USAGE: simple_http_server <path_to_www_root>\n");
        return 1;
    }

    signal(SIGCHLD, SIG_IGN);
    
    Http_server_t s;

    //Listen for tcp connection
    s.tcp_listener_fd = create_tcp_listener(IP, PORT);
    while (1) {
        s.tcp_connection = accept_tcp_connection(s.tcp_listener_fd);
        if (fork()) { //It should be a thread pool instead of fork but ...
            close_tcp_connection(&s.tcp_connection);
            continue; //Parent keeps listening
        }
        else {
            close_tcp_listener(s.tcp_listener_fd);
            break; //Child deals with the established connection
        }
    }
    
    //Create reader
    s.input_queue = init_reader(s.tcp_connection.fd);
    
    //For now loop indefinitely
    while (1) {
        //Parse next request
        Http_message_t req;
        init_http_message(&req, HTTP_REQUEST);
        Http_status_t status = parse_http_request(&req, s.input_queue);
        
        echo_request(&req);

        //Prepare response
        Http_message_t res;
        init_http_message(&res, HTTP_RESPONSE);
        char* body = NULL;
        char size[1024];
        size_t body_len = 0;
        switch (status) {
            case HTTP_STATUS_OK:
                {
                    char route[PATH_MAX];
                    status = route_http_request(&req, route, www_root);
                    FILE* f = fopen(route, "rb");
                    if (!f) {
                        LOG(ERROR, "fopen failed!");
                        exit(1);
                    }
                    long fs = get_file_size(f);
                    if (fs == 0) {
                        fclose(f);
                        break;
                    }
                    body = malloc(fs);
                    if (!body) {
                        LOG(ERROR, "malloc failed!");
                        exit(1);
                    }
                    if (fread(body, 1, fs, f) < fs) {
                        LOG(ERROR, "fread failed!");
                        exit(1);
                    }
                    fclose(f);
                    body_len = fs;
                }
                break;
            default:
                {
                    
                }
                break;
        }
        char status_char[4];
        sprintf(status_char, "%d", status);
        write_response_status_line(&res, "HTTP/1.1",
                status_char, (char*)http_status_to_string(status));
        if (body_len) {
            sprintf(size, "%zu", body_len);
            write_response_field_line(&res, "Content-length", size);
            write_response_body_content_length(&res, body, body_len);
        }

        //Send response
        send_response(s.tcp_connection, &res);

        echo_response(&res);

        //Cleanup
        free_http_message(&req);
        free_http_message(&res);
    }

    close_tcp_connection(&s.tcp_connection);
    free_input_queue(s.input_queue);
    fflush(stdout);
    return 0;
}

