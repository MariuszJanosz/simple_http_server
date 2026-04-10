#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <threads.h>

#include <unistd.h>

#include "log.h"
#include "stream_reader.h"
#include "tcp_connection.h"

#define IP (((unsigned int)127*(1<<24))+(0*(1<<16))+(0*(1<<8))+1) //127.0.0.1 localhost
#define PORT 54321

int main() {
    //Listen for tcp connection
    int tcp_listener_fd = create_tcp_listener(IP, PORT);
    Tcp_connection_t tcp_connection = accept_tcp_connection(tcp_listener_fd);
    
    //Create in_stream reader
    Input_queue_t* input_queue = init_stream_reader(tcp_connection.in_stream);

    //Read incoming data
    while (!is_reading_finished(input_queue)) {
        char buffer[1024];
        int read = get_data(input_queue, buffer, 1024);
        if (read == 0)
            continue;
        printf("%.*s", read, buffer);
        fprintf(tcp_connection.out_stream, "Server echo: %.*s", read, buffer);
        fflush(tcp_connection.out_stream);
    }

    close_tcp_connection(&tcp_connection);
    free_input_queue(input_queue);
    fflush(stdout);
    return 0;
}

