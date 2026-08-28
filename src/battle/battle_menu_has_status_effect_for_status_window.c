#include "fft/battle.h"

s32 battle_menu_has_status_effect_for_status_window(s32 unit_id) {
    battle_stats_t* unit;
    s32 index;

    unit = battle_unit_get_stats_from_battle_id(unit_id);
    for (index = 0; index < BATTLE_STATUS_BYTE_COUNT; index++) {
        if (unit->status_sets.current[index] != 0) {
            return 1;
        }
    }
    return 0;
}
