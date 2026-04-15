#include "picocalc_reset_policy.h"

#include <stddef.h>

#include "trs.h"
#include "trs_memory.h"

void picocalc_apply_post_reset_policy(void)
{
    static const int model3_datetime_cache_addrs[] = {
        0x442f, /* LDOS3_MONTH */
        0x4457, /* LDOS3_DAY */
        0x4413, /* LDOS3_YEAR */
        0x42cb, /* NEWDOS3_DATETIME_VALID_ADDR */
        0x42d1, /* NEWDOS3_MONTH */
        0x42d0, /* NEWDOS3_DAY */
        0x42cf, /* NEWDOS3_YEAR */
        0x42ce, /* NEWDOS3_HOUR */
        0x42cd, /* NEWDOS3_MIN */
        0x42cc  /* NEWDOS3_SEC */
    };
    size_t i;

    if (trs_model != 3) {
        return;
    }

    for (i = 0; i < (sizeof(model3_datetime_cache_addrs) / sizeof(model3_datetime_cache_addrs[0])); ++i) {
        mem_poke(model3_datetime_cache_addrs[i], 0);
    }
}
