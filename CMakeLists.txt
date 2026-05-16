cmake_minimum_required(VERSION 3.11)

project(simple_http_server)

set(CMAKE_COMPILE_WARNING_AS_ERROR ON)
set(CMAKE_LINK_WARNING_AS_ERROR ON)

add_executable(simple_http_server)

target_sources(simple_http_server PRIVATE	simple_http_server.c
									log.h
						reader.c	 	reader.h
						tcp_connection.c	tcp_connection.h
						http_message.c		http_message.h
						abnf.c			abnf.h
						uri.c			uri.h
						http_routing.c		http_routing.h
						http_request_queue.c	http_request_queue.h
						http_request_handler.c	http_request_handler.h
						http_field_line.c	http_field_line.h)

install(TARGETS simple_http_server RUNTIME)

#include(FetchContent)
#FetchContent_Declare(
#	unity
#	GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
#	GIT_TAG cbcd08fa7de711053a3deec6339ee89cad5d2697 # v2.6.1
#)
#FetchContent_MakeAvailable(unity)
#
#include(CTest)
#add_subdirectory(tests)

