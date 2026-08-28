#include "fft/battle.h"
#include "psx/types.h"

/* Fire loses and lightning gains a quarter of its HP damage in a
 * thunderstorm; ice gains a quarter in a snowstorm.  Plain rain and snow
 * have no effect. */
void battle_formula_apply_weather_elemental_effects(void) {
    u8 element;
    s32 weather;

    element = g_current_ability.range_data.element;
    weather = battle_map_get_effective_weather();
    if (weather == BATTLE_WEATHER_STORM || weather == BATTLE_WEATHER_STRONG_STORM) {
        if (element & BATTLE_ELEMENT_FIRE) {
            *(s16*)&g_battle_action_target_data->hp_damage = *(s16*)&g_battle_action_target_data->hp_damage * 3 / 4;
        }
        if (element & BATTLE_ELEMENT_LIGHTNING) {
            *(s16*)&g_battle_action_target_data->hp_damage = *(s16*)&g_battle_action_target_data->hp_damage * 5 / 4;
        }
    }
    if (weather == BATTLE_WEATHER_SNOWSTORM || weather == BATTLE_WEATHER_STRONG_SNOWSTORM) {
        if (element & BATTLE_ELEMENT_ICE) {
            *(s16*)&g_battle_action_target_data->hp_damage = *(s16*)&g_battle_action_target_data->hp_damage * 5 / 4;
        }
    }
}
