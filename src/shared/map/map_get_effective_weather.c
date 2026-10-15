#include "fft/battle_map.h"
#include "fft/battle_script.h"
#include "fft/script.h"
#include "psx/types.h"

/* Current weather as seen by damage and movement rules: none when the map
 * ignores weather (indoors), otherwise the {3C} strength shifted into the
 * snow range when the map's snow flag is set. */
s32 map_get_effective_weather(void) {
    s32 weather;
    u32 flags;

    weather = script_get_variable(EVENT_SCRIPT_VAR_WEATHER);
    flags = g_map_weather_flags;
    if (flags & MAP_WEATHER_FLAG_IGNORE_WEATHER) {
        return BATTLE_WEATHER_NONE;
    }
    if (weather < BATTLE_WEATHER_RAIN) {
        return weather;
    }
    if (flags & MAP_WEATHER_FLAG_SNOW) {
        weather += BATTLE_WEATHER_SNOW_OFFSET;
    }
    return weather;
}
