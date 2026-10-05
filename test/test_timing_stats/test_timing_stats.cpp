#include <cstdint>
#include <unity.h>

#include "timing_stats.h"

void setUp(void) {}
void tearDown(void) {}

void test_empty_stats_are_zero(void)
{
    TimingStats s;
    TEST_ASSERT_EQUAL_UINT32(0, s.count());
    TEST_ASSERT_EQUAL_UINT32(0, s.min());
    TEST_ASSERT_EQUAL_UINT32(0, s.max());
    TEST_ASSERT_EQUAL_UINT32(0, s.avg());
}

void test_single_value(void)
{
    TimingStats s;
    s.add(250);
    TEST_ASSERT_EQUAL_UINT32(1, s.count());
    TEST_ASSERT_EQUAL_UINT32(250, s.min());
    TEST_ASSERT_EQUAL_UINT32(250, s.max());
    TEST_ASSERT_EQUAL_UINT32(250, s.avg());
}

void test_min_avg_max(void)
{
    TimingStats s;
    s.add(100);
    s.add(300);
    s.add(200);
    TEST_ASSERT_EQUAL_UINT32(3, s.count());
    TEST_ASSERT_EQUAL_UINT32(100, s.min());
    TEST_ASSERT_EQUAL_UINT32(300, s.max());
    TEST_ASSERT_EQUAL_UINT32(200, s.avg());
}

void test_zero_duration_is_a_valid_minimum(void)
{
    TimingStats s;
    s.add(50);
    s.add(0);
    TEST_ASSERT_EQUAL_UINT32(0, s.min());
    TEST_ASSERT_EQUAL_UINT32(50, s.max());
}

void test_large_values_do_not_overflow_the_sum(void)
{
    TimingStats s;
    for (int i = 0; i < 10; i++) {
        s.add(UINT32_MAX);  // the sum exceeds 32 bits
    }
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, s.avg());
}

void test_reset_clears_everything(void)
{
    TimingStats s;
    s.add(10);
    s.add(20);
    s.reset();
    TEST_ASSERT_EQUAL_UINT32(0, s.count());
    s.add(7);
    TEST_ASSERT_EQUAL_UINT32(7, s.min());
    TEST_ASSERT_EQUAL_UINT32(7, s.max());
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_empty_stats_are_zero);
    RUN_TEST(test_single_value);
    RUN_TEST(test_min_avg_max);
    RUN_TEST(test_zero_duration_is_a_valid_minimum);
    RUN_TEST(test_large_values_do_not_overflow_the_sum);
    RUN_TEST(test_reset_clears_everything);
    return UNITY_END();
}
