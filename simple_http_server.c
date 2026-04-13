#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <threads.h>

#include <unistd.h>

#include "log.h"
#include "stream_reader.h"
#include "tcp_connection.h"
#include "http_message.h"

#define IP (((unsigned int)127*(1<<24))+(0*(1<<16))+(0*(1<<8))+1) //127.0.0.1 localhost
#define PORT 54321

int main() {
    //Listen for tcp connection
    int tcp_listener_fd = create_tcp_listener(IP, PORT);
    Tcp_connection_t tcp_connection;
    while (1) {
        tcp_connection = accept_tcp_connection(tcp_listener_fd);
        if (fork()) {
            close_tcp_connection(&tcp_connection);
            continue; //Parent keeps listening
        }
        else {
            close_tcp_listener(tcp_listener_fd);
            break; //Child deals with the established connection
        }
    }
    
    //Create in_stream reader
    Input_queue_t* input_queue = init_stream_reader(tcp_connection.in_stream);
    
    Http_message_t req;
    init_http_message(&req, HTTP_REQUEST);
    int success = parse_request_line(&req, input_queue);
    if (success) {
        Request_line_t* req_line = (Request_line_t*)req.start_line;
        char* method = NULL;
        switch (req_line->method) {
            case HTTP_GET:
                method = "GET";
                break;
            default:
                method = "ERROR";
        }
        printf("Method: %s\n", method);
        printf("Path: %s\n", req_line->request_target);
        printf("Version: %s\n", req_line->http_version);
    }

    close_tcp_connection(&tcp_connection);
    free_input_queue(input_queue);
    fflush(stdout);
    return 0;
}

