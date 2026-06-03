cmake_minimum_required(VERSION 3.11)

project(simple_http_server)

set(CMAKE_COMPILE_WARNING_AS_ERROR ON)
set(CMAKE_LINK_WARNING_AS_ERROR ON)

add_executable(simple_http_server)

target_sources(simple_http_server PRIVATE	simple_http_server.c
										log.h
										status.h
						reader.c	 		reader.h
						tcp_connection.c		tcp_connection.h
						uri/URI/abnf.c			uri/URI/abnf.h
						uri/URI/uri.c			uri/URI/uri.h
						uri/URI/uri_parser.c		uri/URI/uri_parser.h
						http_request_queue.c		http_request_queue.h
						http_common_field_lines.c	http_common_field_lines.h
						http_normalize_field_line.c	http_normalize_field_line.h
						http_field_line.c		http_field_line.h
						http_request.c			http_request.h
						http_request_context.c		http_request_context.h
						http_response.c			http_response.h
						http_routing.c			http_routing.h)

target_include_directories(simple_http_server PRIVATE uri/URI/)

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

