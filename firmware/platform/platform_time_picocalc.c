#include <stdbool.h>
#include <string.h>
#include <time.h>

#include "pico/stdlib.h"

static bool picocalc_time_initialized;
static time_t picocalc_time_base_epoch;

static int month_from_abbrev(const char *month)
{
    static const char *months[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    int index;

    for (index = 0; index < 12; ++index) {
        if (strncmp(month, months[index], 3) == 0) {
            return index;
        }
    }

    return 0;
}

static time_t build_epoch_fallback(void)
{
    struct tm tm_value;
    char month[4];
    int day;
    int year;
    int hour;
    int minute;
    int second;

    memset(&tm_value, 0, sizeof(tm_value));

    month[0] = __DATE__[0];
    month[1] = __DATE__[1];
    month[2] = __DATE__[2];
    month[3] = '\0';

    day = ((__DATE__[4] == ' ') ? 0 : (__DATE__[4] - '0')) * 10 + (__DATE__[5] - '0');
    year = (__DATE__[7] - '0') * 1000
         + (__DATE__[8] - '0') * 100
         + (__DATE__[9] - '0') * 10
         + (__DATE__[10] - '0');
    hour = (__TIME__[0] - '0') * 10 + (__TIME__[1] - '0');
    minute = (__TIME__[3] - '0') * 10 + (__TIME__[4] - '0');
    second = (__TIME__[6] - '0') * 10 + (__TIME__[7] - '0');

    tm_value.tm_year = year - 1900;
    tm_value.tm_mon = month_from_abbrev(month);
    tm_value.tm_mday = day;
    tm_value.tm_hour = hour;
    tm_value.tm_min = minute;
    tm_value.tm_sec = second;
    tm_value.tm_isdst = -1;

    return mktime(&tm_value);
}

static void ensure_time_initialized(void)
{
    if (picocalc_time_initialized) {
        return;
    }

    picocalc_time_base_epoch = build_epoch_fallback();
    if (picocalc_time_base_epoch < 1704067200) {
        picocalc_time_base_epoch = 1767225600;
    }

    picocalc_time_initialized = true;
}

time_t time(time_t *timer)
{
    time_t now;

    ensure_time_initialized();
    now = picocalc_time_base_epoch + (time_t)(to_ms_since_boot(get_absolute_time()) / 1000u);

    if (timer != NULL) {
        *timer = now;
    }

    return now;
}
