#include "fft/battle.h"
#include "fft/world.h"

/* Target 0x800f4f94. */
void world_formation_apply_auto_battle_selection(u8 target, s32 unit_id) {
    battle_stats_t* unit = battle_unit_get_stats_from_battle_id(unit_id);

    unit->auto_battle_target = target;
    unit->auto_battle_setting = g_world_formation_selected_auto_battle_setting;
}
