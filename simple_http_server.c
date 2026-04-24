#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <threads.h>
#include <signal.h>
#include <stdint.h>

#include <unistd.h>

#include "log.h"
#include "stream_reader.h"
#include "tcp_connection.h"
#include "http_message.h"

#define IP (((unsigned int)127*(1<<24))+(0*(1<<16))+(0*(1<<8))+1) //127.0.0.1 localhost
#define PORT 54321

typedef struct Http_server_t {
    int tcp_listener_fd;
    Tcp_connection_t tcp_connection;
    Input_queue_t* input_queue;
} Http_server_t;

int main() {
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
    
    //Create in_stream reader
    s.input_queue = init_stream_reader(s.tcp_connection.in_stream);
    
    Http_message_t req;
    init_http_message(&req, HTTP_REQUEST);
    Http_status_t status = parse_http_request(&req, s.input_queue);
    Request_line_t* req_line = (Request_line_t*)req.start_line;
    printf("---req---echo---\n");
    printf("%s %s %s\n",
    http_method_to_string(req_line->method),
    req_line->request_target,
    req_line->http_version);
    for (int i = 0; i < req.field_lines_count; ++i) {
        printf("%s: %s\n", req.field_lines[i].field_name, req.field_lines[i].field_value);
    }
    printf("\n");
    if (req.message_body) {
        printf("%.*s\n", (int)req.body_size, req.message_body);
        printf("\n");
    }
    switch (status) {
        case HTTP_STATUS_OK:
            {
                const char* res_body = "<h1>Hello</h1>";
                const char* res = "HTTP/1.1 %d %s\r\nContent-Length: %zu\r\n\r\n%s";
                printf("---res---echo---\n");
                printf(res, status, http_status_to_string(status), strlen(res_body), res_body);
                printf("\n");
                fprintf(s.tcp_connection.out_stream, res, status,
                        http_status_to_string(status), strlen(res_body), res_body);
                fflush(s.tcp_connection.out_stream);
            }
            break;
        default:
            {
                const char* res_body = "<h1>Error</h1>";
                const char* res = "HTTP/1.1 %d %s\r\nContent-Length: %zu\r\n\r\n%s";
                printf("---res---echo---\n");
                printf(res, status, http_status_to_string(status), strlen(res_body), res_body);
                printf("\n");
                fprintf(s.tcp_connection.out_stream, res, status,
                        http_status_to_string(status), strlen(res_body), res_body);
                fflush(s.tcp_connection.out_stream);
            }
            break;
    }

    close_tcp_connection(&s.tcp_connection);
    free_input_queue(s.input_queue);
    fflush(stdout);
    return 0;
}

