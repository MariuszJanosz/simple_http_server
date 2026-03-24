cmake_minimum_required(VERSION 3.0)

project(simple_http_server)

set(CMAKE_COMPILE_WARNING_AS_ERROR ON)
set(CMAKE_LINK_WARNING_AS_ERROR ON)

add_executable(simple_http_server)

target_sources(simple_http_server PRIVATE 	simple_http_server.c log.h
						stream_reader.c stream_reader.h)

install(TARGETS simple_http_server RUNTIME)

