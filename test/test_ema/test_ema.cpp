#include <cmath>
#include <unity.h>

#include "ema.h"

void setUp(void) {}
void tearDown(void) {}

void test_first_sample_initialises_filter(void)
{
    Ema f(0.2f);
    TEST_ASSERT_FALSE(f.primed());
    TEST_ASSERT_EQUAL_FLOAT(23.5f, f.update(23.5f));
    TEST_ASSERT_TRUE(f.primed());
}

void test_second_sample_uses_formula(void)
{
    Ema f(0.2f);
    f.update(10.0f);
    // 0.2 * 20 + 0.8 * 10 = 12
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 12.0f, f.update(20.0f));
}

void test_constant_input_stays_constant(void)
{
    Ema f(0.2f);
    for (int i = 0; i < 50; i++) {
        TEST_ASSERT_FLOAT_WITHIN(1e-4f, 21.0f, f.update(21.0f));
    }
}

void test_step_response_matches_closed_form(void)
{
    // Step 0 -> 1 after the first sample: y[n] = 1 - (1 - alpha)^n.
    const float alpha = 0.2f;
    Ema f(alpha);
    f.update(0.0f);
    float y = 0.0f;
    for (int n = 1; n <= 10; n++) {
        y = f.update(1.0f);
        const float expected = 1.0f - std::pow(1.0f - alpha, (float)n);
        TEST_ASSERT_FLOAT_WITHIN(1e-4f, expected, y);
    }
}

void test_step_response_is_monotonic_and_bounded(void)
{
    Ema f(0.05f);
    f.update(0.0f);
    float prev = 0.0f;
    for (int n = 0; n < 200; n++) {
        const float y = f.update(1.0f);
        TEST_ASSERT_TRUE(y >= prev);
        TEST_ASSERT_TRUE(y <= 1.0f);
        prev = y;
    }
}

void test_larger_alpha_reacts_faster(void)
{
    Ema slow(0.05f), mid(0.2f), fast(0.5f);
    slow.update(0.0f);
    mid.update(0.0f);
    fast.update(0.0f);
    float s = 0, m = 0, fa = 0;
    for (int i = 0; i < 5; i++) {
        s = slow.update(1.0f);
        m = mid.update(1.0f);
        fa = fast.update(1.0f);
    }
    TEST_ASSERT_TRUE(s < m);
    TEST_ASSERT_TRUE(m < fa);
}

void test_alpha_one_is_pass_through(void)
{
    Ema f(1.0f);
    f.update(5.0f);
    TEST_ASSERT_EQUAL_FLOAT(9.0f, f.update(9.0f));
}

void test_invalid_alpha_disables_filtering(void)
{
    Ema zero(0.0f), negative(-0.3f), big(1.5f), nan_alpha(NAN);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, zero.alpha());
    TEST_ASSERT_EQUAL_FLOAT(1.0f, negative.alpha());
    TEST_ASSERT_EQUAL_FLOAT(1.0f, big.alpha());
    TEST_ASSERT_EQUAL_FLOAT(1.0f, nan_alpha.alpha());
}

void test_non_finite_sample_is_ignored(void)
{
    Ema f(0.2f);
    f.update(10.0f);
    TEST_ASSERT_EQUAL_FLOAT(10.0f, f.update(NAN));
    TEST_ASSERT_EQUAL_FLOAT(10.0f, f.update(INFINITY));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 12.0f, f.update(20.0f));  // state was not corrupted
}

void test_non_finite_first_sample_does_not_prime(void)
{
    Ema f(0.2f);
    f.update(NAN);
    TEST_ASSERT_FALSE(f.primed());
    TEST_ASSERT_EQUAL_FLOAT(7.0f, f.update(7.0f));
}

void test_reset_forgets_history(void)
{
    Ema f(0.2f);
    f.update(100.0f);
    f.update(100.0f);
    f.reset();
    TEST_ASSERT_FALSE(f.primed());
    TEST_ASSERT_EQUAL_FLOAT(3.0f, f.update(3.0f));
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_first_sample_initialises_filter);
    RUN_TEST(test_second_sample_uses_formula);
    RUN_TEST(test_constant_input_stays_constant);
    RUN_TEST(test_step_response_matches_closed_form);
    RUN_TEST(test_step_response_is_monotonic_and_bounded);
    RUN_TEST(test_larger_alpha_reacts_faster);
    RUN_TEST(test_alpha_one_is_pass_through);
    RUN_TEST(test_invalid_alpha_disables_filtering);
    RUN_TEST(test_non_finite_sample_is_ignored);
    RUN_TEST(test_non_finite_first_sample_does_not_prime);
    RUN_TEST(test_reset_forgets_history);
    return UNITY_END();
}
