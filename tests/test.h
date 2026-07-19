#ifndef DES_TEST_H
#define DES_TEST_H

#include <stdio.h>
#include <string.h>

static int des_tests_run;
static int des_tests_failed;
static int des_current_test_failed;

static void des_test_fail(const char *file, int line, const char *message) {
    fprintf(stderr, "%s:%d: %s\n", file, line, message);
    des_current_test_failed = 1;
}

#define UNITY_BEGIN() (des_tests_run = 0, des_tests_failed = 0, 0)
#define UNITY_END() (printf("%d tests, %d failures\n", des_tests_run, des_tests_failed), des_tests_failed)
#define RUN_TEST(fn) do { \
    des_current_test_failed = 0; setUp(); fn(); tearDown(); des_tests_run++; \
    if (des_current_test_failed) { des_tests_failed++; fprintf(stderr, "  FAIL %s\n", #fn); } \
    else printf("  PASS %s\n", #fn); \
} while (0)

#define TEST_ASSERT_TRUE(value) do { if (!(value)) { des_test_fail(__FILE__, __LINE__, "expected true: " #value); return; } } while (0)
#define TEST_ASSERT_FALSE(value) do { if ((value)) { des_test_fail(__FILE__, __LINE__, "expected false: " #value); return; } } while (0)
#define TEST_ASSERT_NULL(value) do { if ((value) != NULL) { des_test_fail(__FILE__, __LINE__, "expected null: " #value); return; } } while (0)
#define TEST_ASSERT_NOT_NULL(value) do { if ((value) == NULL) { des_test_fail(__FILE__, __LINE__, "expected non-null: " #value); return; } } while (0)
#define TEST_ASSERT_EQUAL_INT(expected, actual) do { \
    long long des_expected = (long long)(expected); long long des_actual = (long long)(actual); \
    if (des_expected != des_actual) { char des_message[256]; snprintf(des_message, sizeof(des_message), "expected %lld but got %lld", des_expected, des_actual); des_test_fail(__FILE__, __LINE__, des_message); return; } \
} while (0)
#define TEST_ASSERT_EQUAL_STRING(expected, actual) do { \
    const char *des_expected = (expected); const char *des_actual = (actual); \
    if (!des_expected || !des_actual || strcmp(des_expected, des_actual) != 0) { char des_message[256]; snprintf(des_message, sizeof(des_message), "expected string '%s' but got '%s'", des_expected ? des_expected : "(null)", des_actual ? des_actual : "(null)"); des_test_fail(__FILE__, __LINE__, des_message); return; } \
} while (0)

#endif
