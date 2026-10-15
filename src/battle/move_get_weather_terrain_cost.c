#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_ai.h"
#include "fft/battle_map.h"
#include "fft/battle_move.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "fft/memory.h"
#include "psx/libetc.h"
#include "psx/types.h"

s32 move_get_weather_terrain_cost(void) {
    s32 weather;

    weather = map_get_effective_weather();
    if (weather < BATTLE_WEATHER_STORM || weather == BATTLE_WEATHER_SNOW) {
        return 1;
    }
    if (weather == BATTLE_WEATHER_STORM || weather == BATTLE_WEATHER_SNOWSTORM) {
        return 2;
    }
    return 3;
}
