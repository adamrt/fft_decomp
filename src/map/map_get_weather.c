#include "fft/map.h"
#include "fft/script.h"
#include "psx/types.h"

/* The raw weather script variable. map_get_effective_weather applies the
 * map's indoor and snow flags to it. */
s32 map_get_weather(void) {
    return g_script_variables[EVENT_SCRIPT_VAR_WEATHER];
}
