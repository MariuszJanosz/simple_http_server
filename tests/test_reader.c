#include "unity.h"
#include "reader.h"

#include <stdlib.h>
#include <stdio.h>

#include <unistd.h>

Input_queue_t* iq = NULL;

void setUp() {
    iq = malloc(sizeof(*iq));
    if (!iq) {
        fprintf(stderr, "malloc failed!");
        exit(1);
    }
    init_input_queue(iq);
}

void tearDown() {
    free_input_queue(iq);
    iq = NULL;
}

void test_init_input_queue() {
    TEST_ASSERT_EQUAL_INT_MESSAGE(-1, iq->front, "Incorrect front!");
    TEST_ASSERT_EQUAL_INT_MESSAGE(-1, iq->rear, "Incorrect rear!");
    TEST_ASSERT_EQUAL_INT_MESSAGE( 0, iq->reached_eof, "Incorrect reached_eof!");
    TEST_ASSERT_EQUAL_INT_MESSAGE( 0, iq->is_nonempty, "Incorrect is_nonempty!");
    TEST_ASSERT_EQUAL_INT_MESSAGE( 1, iq->is_nonfull, "Incorrect is_nonfull!");
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_init_input_queue);
    return UNITY_END();
}

