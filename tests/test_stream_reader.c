#include "unity.h"
#include "stream_reader.h"

#include <stdlib.h>
#include <stdio.h>

#include <unistd.h>

Line_queue_t* lq = NULL;

void setUp() {
    lq = malloc(sizeof(*lq));
    if (!lq) {
        fprintf(stderr, "malloc failed!");
        exit(1);
    }
    init_line_queue(lq);
}

void tearDown() {
    free_line_queue(lq);
    lq = NULL;
}

void test_init_line_queue() {
    TEST_ASSERT_EQUAL_INT_MESSAGE(-1, lq->front, "Incorrect front!");
    TEST_ASSERT_EQUAL_INT_MESSAGE(-1, lq->rear, "Incorrect rear!");
    TEST_ASSERT_EQUAL_INT_MESSAGE( 0, lq->reached_eof, "Incorrect reached_eof!");
    TEST_ASSERT_EQUAL_INT_MESSAGE( 0, lq->is_nonempty, "Incorrect is_nonempty!");
    TEST_ASSERT_EQUAL_INT_MESSAGE( 1, lq->is_nonfull, "Incorrect is_nonfull!");
}

char* read_line(int fd);
void test_read_line() {
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        fprintf(stderr, "pipe failed!");
        exit(1);
    }
    char buff[] =   "Line 1\n"
                    "Line 2\n"
                    "Line 3\n";
    write(pipefd[1], buff, sizeof(buff)/sizeof(buff[0]));
    close(pipefd[1]);
    char* line = read_line(pipefd[0]);
    TEST_ASSERT_EQUAL_STRING("Line 1\n", line);
    free(line);
    line = read_line(pipefd[0]);
    TEST_ASSERT_EQUAL_STRING("Line 2\n", line);
    free(line);
    line = read_line(pipefd[0]);
    TEST_ASSERT_EQUAL_STRING("Line 3\n", line);
    free(line);
    line = read_line(pipefd[0]);
    TEST_ASSERT_EQUAL_STRING("", line);
    free(line);
    close(pipefd[0]);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_init_line_queue);
    RUN_TEST(test_read_line);
    return UNITY_END();
}

