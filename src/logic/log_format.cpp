#include "log_format.h"

#include <cstdio>

size_t format_log_line(const LogEntry &e, char *buf, size_t size)
{
    if (buf == nullptr || size == 0) {
        return 0;
    }
    const long secs_of_day = (long)(((e.ts % 86400) + 86400) % 86400);
    const int hh = (int)(secs_of_day / 3600);
    const int mm = (int)((secs_of_day % 3600) / 60);
    const int ss = (int)(secs_of_day % 60);

    const int n = std::snprintf(buf, size, "[%02d:%02d:%02d] T:%.1f H:%.0f%% P:%.0f ERR:%u%s%s", hh, mm, ss,
                                (double)e.temp_c, (double)e.hum_pct, (double)e.press_hpa, (unsigned)e.err_cnt,
                                (e.flags & LOG_FLAG_TIME_TRUSTED) ? "" : " !TIME",
                                (e.flags & LOG_FLAG_SENSOR_VALID) ? "" : " !SENS");
    if (n < 0 || (size_t)n >= size) {
        buf[0] = '\0';
        return 0;
    }
    return (size_t)n;
}
