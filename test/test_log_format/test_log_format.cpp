#include <cstring>
#include <unity.h>

#include "log_format.h"

void setUp(void) {}
void tearDown(void) {}

static LogEntry make(time_t ts, float t, float h, float p, uint32_t err, uint8_t flags)
{
    LogEntry e = {};
    e.ts = ts;
    e.temp_c = t;
    e.hum_pct = h;
    e.press_hpa = p;
    e.err_cnt = err;
    e.flags = flags;
    return e;
}

void test_matches_brief_format(void)
{
    // 2026-10-05 12:34:56 UTC is 1791203696 s since 1970, i.e. 12:34:56 time of day.
    const LogEntry e = make(1791203696, 23.4f, 48.2f, 1013.2f, 0, LOG_FLAG_SENSOR_VALID | LOG_FLAG_TIME_TRUSTED);
    char buf[96];
    TEST_ASSERT_TRUE(format_log_line(e, buf, sizeof(buf)) > 0);
    TEST_ASSERT_EQUAL_STRING("[12:34:56] T:23.4 H:48% P:1013 ERR:0", buf);
}

void test_midnight_and_end_of_day(void)
{
    char buf[96];
    format_log_line(make(86400 * 100, 0, 0, 0, 0, LOG_FLAG_SENSOR_VALID | LOG_FLAG_TIME_TRUSTED), buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING_LEN("[00:00:00]", buf, 10);
    format_log_line(make(86400 * 100 + 86399, 0, 0, 0, 0, LOG_FLAG_SENSOR_VALID | LOG_FLAG_TIME_TRUSTED), buf,
                    sizeof(buf));
    TEST_ASSERT_EQUAL_STRING_LEN("[23:59:59]", buf, 10);
}

void test_negative_temperature(void)
{
    char buf[96];
    format_log_line(make(0, -5.25f, 80.0f, 990.0f, 3, LOG_FLAG_SENSOR_VALID | LOG_FLAG_TIME_TRUSTED), buf, sizeof(buf));
    TEST_ASSERT_NOT_NULL(strstr(buf, "T:-5.2"));  // one decimal (-5.25 rounds to -5.2 or -5.3)
    TEST_ASSERT_NOT_NULL(strstr(buf, "ERR:3"));
}

void test_untrusted_time_is_marked(void)
{
    char buf[96];
    format_log_line(make(1000, 20.0f, 40.0f, 1000.0f, 0, LOG_FLAG_SENSOR_VALID), buf, sizeof(buf));
    TEST_ASSERT_NOT_NULL(strstr(buf, " !TIME"));
    TEST_ASSERT_NULL(strstr(buf, " !SENS"));
}

void test_invalid_sensor_is_marked(void)
{
    char buf[96];
    format_log_line(make(1000, 20.0f, 40.0f, 1000.0f, 2, LOG_FLAG_TIME_TRUSTED), buf, sizeof(buf));
    TEST_ASSERT_NOT_NULL(strstr(buf, " !SENS"));
    TEST_ASSERT_NULL(strstr(buf, " !TIME"));
}

void test_both_markers(void)
{
    char buf[96];
    format_log_line(make(1000, 0, 0, 0, 0, 0), buf, sizeof(buf));
    TEST_ASSERT_NOT_NULL(strstr(buf, " !TIME !SENS"));
}

void test_buffer_too_small_returns_zero(void)
{
    char buf[10];
    const LogEntry e = make(1791203696, 23.4f, 48.2f, 1013.2f, 0, LOG_FLAG_SENSOR_VALID | LOG_FLAG_TIME_TRUSTED);
    TEST_ASSERT_EQUAL(0, format_log_line(e, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_CHAR('\0', buf[0]);
}

void test_invalid_buffer_arguments(void)
{
    const LogEntry e = make(0, 0, 0, 0, 0, 0);
    char buf[8];
    TEST_ASSERT_EQUAL(0, format_log_line(e, nullptr, 96));
    TEST_ASSERT_EQUAL(0, format_log_line(e, buf, 0));
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_matches_brief_format);
    RUN_TEST(test_midnight_and_end_of_day);
    RUN_TEST(test_negative_temperature);
    RUN_TEST(test_untrusted_time_is_marked);
    RUN_TEST(test_invalid_sensor_is_marked);
    RUN_TEST(test_both_markers);
    RUN_TEST(test_buffer_too_small_returns_zero);
    RUN_TEST(test_invalid_buffer_arguments);
    return UNITY_END();
}
