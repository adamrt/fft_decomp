#include "fft/battle.h"

void battle_script_blueremoveunit_all_enemy_units(void) {
    s32 i;
    s32 idx;
    s32 out_index;
    battle_stats_t* stats;
    battle_stats_t* found;

    i = 0;
    do {
        if (battle_unit_has_misc_id(i) != 0) {
            idx = battle_unit_get_battle_index_by_misc_id(i);
            if (idx != -1) {
                stats = battle_unit_get_stats_from_battle_id(idx);
                found = battle_find_unit_data_pointer_for_entd_unit_id(stats->unit_id, &out_index);
                if ((found->initial_team_flags & BATTLE_TEAM_MASK) && out_index != -2) {
                    battle_gfx_init_misc_unit_palette_modulation(i, 2, -0x1F, -0x1F, 0);
                }
            }
        }
        i++;
    } while (i < EVENT_UNIT_SLOT_COUNT);
}
