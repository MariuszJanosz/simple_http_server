#include "unity.h"
#include "http_message.h"

#include <string.h>
#include <stdlib.h>

void setUp() {}
void tearDown() {}

void test_init_http_message() {
    Http_message_t msg;
    init_http_message(&msg, HTTP_REQUEST);
    TEST_ASSERT_EQUAL_INT(HTTP_REQUEST, msg.message_type);
    TEST_ASSERT_NULL(msg.start_line);
    TEST_ASSERT_NULL(msg.field_lines);
    TEST_ASSERT_EQUAL_INT(0, msg.field_lines_count);
    TEST_ASSERT_NULL(msg.message_body);

    init_http_message(&msg, HTTP_RESPONSE);
    TEST_ASSERT_EQUAL_INT(HTTP_RESPONSE, msg.message_type);
    TEST_ASSERT_NULL(msg.start_line);
    TEST_ASSERT_NULL(msg.field_lines);
    TEST_ASSERT_EQUAL_INT(0, msg.field_lines_count);
    TEST_ASSERT_NULL(msg.message_body);
}

void test_parse_request_line() {
    Http_message_t msg;
    init_http_message(&msg, HTTP_REQUEST);
    char* line = strdup("GET /abc HTTP/1.1\r\n");
    int success = parse_request_line(&msg, line);
    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_EQUAL_INT(HTTP_REQUEST, msg.message_type);
    TEST_ASSERT_EQUAL_INT(HTTP_GET,
    ((Request_line_t*)msg.start_line)->method);
    TEST_ASSERT_EQUAL_STRING("/abc",
    ((Request_line_t*)msg.start_line)->request_target);
    TEST_ASSERT_EQUAL_STRING("HTTP/1.1",
    ((Request_line_t*)msg.start_line)->http_version);
    free(((Request_line_t*)msg.start_line)->request_target);
    free(((Request_line_t*)msg.start_line)->http_version);
    free(msg.start_line);
    line = strdup("yfhsaod oifesdv");
    success = parse_request_line(&msg, line);
    TEST_ASSERT_FALSE(success);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_init_http_message);
    RUN_TEST(test_parse_request_line);
    return UNITY_END();
}

