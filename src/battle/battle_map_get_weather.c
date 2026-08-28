#include "fft/battle.h"
#include "fft/script_variables.h"
#include "psx/types.h"

s32 battle_map_get_weather(void) {
    return g_battle_script_variables[EVENT_SCRIPT_VAR_WEATHER];
}
