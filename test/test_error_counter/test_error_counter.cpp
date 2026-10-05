#include <cstdint>
#include <unity.h>

#include "error_counter.h"

void setUp(void) {}
void tearDown(void) {}

void test_starts_at_zero(void)
{
    ErrorCounter c;
    TEST_ASSERT_EQUAL_UINT32(0, c.value());
}

void test_increment_returns_new_value(void)
{
    ErrorCounter c;
    TEST_ASSERT_EQUAL_UINT32(1, c.increment());
    TEST_ASSERT_EQUAL_UINT32(2, c.increment());
    TEST_ASSERT_EQUAL_UINT32(2, c.value());
}

void test_saturates_instead_of_wrapping(void)
{
    ErrorCounter c(UINT32_MAX - 1);
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, c.increment());
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, c.increment());  // would wrap to 0 without saturation
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, c.value());
}

void test_reset_clears_and_counting_resumes(void)
{
    ErrorCounter c(5);
    c.reset();
    TEST_ASSERT_EQUAL_UINT32(0, c.value());
    TEST_ASSERT_EQUAL_UINT32(1, c.increment());
}

void test_reset_after_saturation(void)
{
    ErrorCounter c(UINT32_MAX);
    c.reset();
    TEST_ASSERT_EQUAL_UINT32(1, c.increment());
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_starts_at_zero);
    RUN_TEST(test_increment_returns_new_value);
    RUN_TEST(test_saturates_instead_of_wrapping);
    RUN_TEST(test_reset_clears_and_counting_resumes);
    RUN_TEST(test_reset_after_saturation);
    return UNITY_END();
}
