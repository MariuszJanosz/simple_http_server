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

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_init_http_message);
    return UNITY_END();
}

