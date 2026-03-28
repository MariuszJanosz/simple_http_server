#include "unity.h"
#include "stream_reader.h"

void setUp() {

}

void tearDown() {

}

void test_init_line_queue() {
    Line_queue_t lq;
    init_line_queue(&lq);
    TEST_ASSERT_EQUAL_INT_MESSAGE(-1, lq.front, "Incorrect front!");
}

void test_free_line_queue() {

}

void stream_reader_thr() {

}

void test_get_line() {

}

void test_is_reading_finished() {

}

void init_stream_reader() {

}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_init_line_queue);
    RUN_TEST(test_free_line_queue);
    RUN_TEST(test_stream_reader_thr);
    RUN_TEST(test_get_line);
    RUN_TEST(test_is_reading_finished);
    RUN_TEST(test_init_stream_reader);
    return UNITY_END();
}

