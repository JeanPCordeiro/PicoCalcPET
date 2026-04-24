#include "SDL.h"

#ifdef PICOCALC_PLATFORM
#include "pico/time.h"
#endif

Uint32 SDL_GetTicks(void)
{
#ifdef PICOCALC_PLATFORM
    return (Uint32)to_ms_since_boot(get_absolute_time());
#else
    return 0;
#endif
}

void SDL_Delay(Uint32 ms)
{
#ifdef PICOCALC_PLATFORM
    sleep_ms(ms);
#else
    (void)ms;
#endif
}
