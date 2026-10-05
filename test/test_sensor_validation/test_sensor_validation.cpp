#include <cmath>
#include <unity.h>

#include "sensor_validation.h"

void setUp(void) {}
void tearDown(void) {}

void test_typical_room_values_are_ok(void)
{
    TEST_ASSERT_EQUAL(SampleStatus::Ok, validate_sample(23.5f, 40.0f, 982.0f));
}

void test_limits_are_inclusive(void)
{
    TEST_ASSERT_EQUAL(SampleStatus::Ok, validate_sample(-40.0f, 0.0f, 300.0f));
    TEST_ASSERT_EQUAL(SampleStatus::Ok, validate_sample(85.0f, 100.0f, 1100.0f));
}

void test_just_outside_the_limits_is_rejected(void)
{
    TEST_ASSERT_EQUAL(SampleStatus::TemperatureOutOfRange, validate_sample(-40.01f, 40.0f, 982.0f));
    TEST_ASSERT_EQUAL(SampleStatus::TemperatureOutOfRange, validate_sample(85.01f, 40.0f, 982.0f));
    TEST_ASSERT_EQUAL(SampleStatus::HumidityOutOfRange, validate_sample(23.0f, -0.01f, 982.0f));
    TEST_ASSERT_EQUAL(SampleStatus::HumidityOutOfRange, validate_sample(23.0f, 100.01f, 982.0f));
    TEST_ASSERT_EQUAL(SampleStatus::PressureOutOfRange, validate_sample(23.0f, 40.0f, 299.9f));
    TEST_ASSERT_EQUAL(SampleStatus::PressureOutOfRange, validate_sample(23.0f, 40.0f, 1100.1f));
}

void test_zero_pressure_from_a_failed_compensation_is_rejected(void)
{
    // The compensation returns 0 instead of dividing by zero; it must not reach the filter.
    TEST_ASSERT_EQUAL(SampleStatus::PressureOutOfRange, validate_sample(23.0f, 40.0f, 0.0f));
}

void test_non_finite_values_are_rejected(void)
{
    TEST_ASSERT_EQUAL(SampleStatus::NotFinite, validate_sample(NAN, 40.0f, 982.0f));
    TEST_ASSERT_EQUAL(SampleStatus::NotFinite, validate_sample(23.0f, INFINITY, 982.0f));
    TEST_ASSERT_EQUAL(SampleStatus::NotFinite, validate_sample(23.0f, 40.0f, -INFINITY));
}

void test_first_problem_is_reported(void)
{
    // everything wrong: temperature is checked first
    TEST_ASSERT_EQUAL(SampleStatus::TemperatureOutOfRange, validate_sample(200.0f, 500.0f, 5.0f));
}

void test_names_are_defined_for_every_status(void)
{
    TEST_ASSERT_EQUAL_STRING("ok", sample_status_name(SampleStatus::Ok));
    TEST_ASSERT_EQUAL_STRING("not a finite number", sample_status_name(SampleStatus::NotFinite));
    TEST_ASSERT_EQUAL_STRING("temperature out of range", sample_status_name(SampleStatus::TemperatureOutOfRange));
    TEST_ASSERT_EQUAL_STRING("humidity out of range", sample_status_name(SampleStatus::HumidityOutOfRange));
    TEST_ASSERT_EQUAL_STRING("pressure out of range", sample_status_name(SampleStatus::PressureOutOfRange));
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_typical_room_values_are_ok);
    RUN_TEST(test_limits_are_inclusive);
    RUN_TEST(test_just_outside_the_limits_is_rejected);
    RUN_TEST(test_zero_pressure_from_a_failed_compensation_is_rejected);
    RUN_TEST(test_non_finite_values_are_rejected);
    RUN_TEST(test_first_problem_is_reported);
    RUN_TEST(test_names_are_defined_for_every_status);
    return UNITY_END();
}
