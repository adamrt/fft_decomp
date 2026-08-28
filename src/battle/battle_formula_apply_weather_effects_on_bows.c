#include "fft/battle.h"
#include "fft/data.h"
#include "fft/main_runtime.h"
#include "fft/script_variables.h"
#include "psx/types.h"

void battle_formula_apply_weather_effects_on_bows(void) {
    u8* base_hit;
    s32 weather;
    s32 time_of_day;

    if ((u32)(g_main_item_primary_data[g_current_ability.weapon_id].type - ITEM_TYPE_CROSSBOW) < 2) {
        weather = battle_script_get_variable(EVENT_SCRIPT_VAR_WEATHER);
        time_of_day = battle_script_get_variable(EVENT_SCRIPT_VAR_TIME_OF_DAY);
        if (time_of_day == 1) { /* night */
            g_current_ability.base_hit = g_current_ability.base_hit * 3 / 4;
        }
        /* storm or strong storm; snow is not folded in here */
        if ((u32)(weather - BATTLE_WEATHER_STORM) < 2) {
            base_hit = &g_current_ability.base_hit;
            *base_hit = *base_hit * 3 / 4;
        }
    }
}
