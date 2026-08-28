#include "fft/world.h"
#include "psx/types.h"

enum { WORLD_WEATHER_WORD_INDEX = 0x23 };

s32 world_map_get_weather(void) {
    return g_world_script_variables[WORLD_WEATHER_WORD_INDEX];
}
