#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <limits.h>
#include <stdint.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>

#include "log.h"
#include "tcp_connection.h"
#include "http_resources.h"
#include "http_request_context.h"
#include "http_response.h"
#include "reader.h"

char g_www_root[PATH_MAX];
size_t g_root_len;

struct in_addr addr;
uint16_t port = 0;

void echo_request_response_pair(Http_request_context_t* req_con, Http_response_t* res);
void process_command_line_arguments(int argc, char** argv);

int main(int argc, char** argv) {
    process_command_line_arguments(argc, argv);
    init_resources_from_config();
    //Do not wait for children return values, prevents zombie processes
    signal(SIGCHLD, SIG_IGN);

    //Listen for tcp connection
    int tcp_listener_fd = create_tcp_listener(addr.s_addr, port);
    Tcp_connection_t tcp_connection;
    while (1) {
        tcp_connection = accept_tcp_connection(tcp_listener_fd);
        if (fork()) { //It should be a thread pool instead of fork but ...
            close_tcp_connection(&tcp_connection);
            continue; //Parent keeps listening
        }
        else {
            close_tcp_listener(tcp_listener_fd);
            break; //Child deals with the established connection
        }
    }

    Http_request_context_t req_con;
    init_request_context(&req_con);
    while (!is_reading_finished(tcp_connection)) {
        req_con.status = parse_http_request(&req_con.req, tcp_connection);
        if (req_con.status == PARSING_BROKEN_CLOSE_CONNECTION) {
            abort_reading(tcp_connection);
        }
        req_con.status == process_request(&req_con);
        Http_response_t res;
        init_response(&res);
        req_con.status = prepare_response(&res, &req_con);
        if (req_con.status != PARSING_BROKEN_CLOSE_CONNECTION) {
            send_response(&res, tcp_connection);
            DEBUG(echo_request_response_pair(&req_con, &res));
        }
        else {
            LOG(INFO, "Parsing broken, closing connection!");
        }
        free_response(&res);
        clean_request_context(&req_con);
    }

    //Cleanup
    free_request_context(&req_con);
    close_tcp_connection(&tcp_connection);
    fflush(stdout);
    return 0;
}

void process_command_line_arguments(int argc, char** argv) {
    if (argc != 4) {
        printf("USAGE: simple_http_server <path_to_www_root> <ip> <port>\n");
        exit(1);
    }
    
    if (!realpath(argv[1], g_www_root)) {
        printf("Invalid path to www root.\n");
        printf("USAGE: simple_http_server <path_to_www_root> <ip> <port>\n");
        exit(1);
    }

    g_root_len = strlen(g_www_root);

    if (!inet_aton(argv[2], &addr)) {
        printf("Invalid ip.\n");
        printf("USAGE: simple_http_server <path_to_www_root> <ip> <port>\n");
        exit(1);
    }

    char* ptr = argv[3];
    while (*ptr) {
        if ('0' <= *ptr && *ptr <= '9') {
            int digit = *ptr - '0';
            if (port > (UINT16_MAX - digit) / 10) {
                printf("Invalid port.\n");
                printf("USAGE: simple_http_server <path_to_www_root> <ip> <port>\n");
                exit(1);
            }
            port = 10 * port + digit;
        }
        else {
            printf("Invalid port.\n");
            printf("USAGE: simple_http_server <path_to_www_root> <ip> <port>\n");
            exit(1);
        }
        ++ptr;
    }
    port = htons(port);
}

void echo_request_response_pair(Http_request_context_t* req_con, Http_response_t* res) {
    printf("---request----\n");
    print_request_context(req_con);
    printf("---response---\n");
    print_response(res);
}

