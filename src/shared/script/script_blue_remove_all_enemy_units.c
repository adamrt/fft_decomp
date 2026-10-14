#include "fft/battle_gfx.h"
#include "fft/battle_script.h"
#include "fft/battle_unit.h"

/* CallFunction 4: tint blue (red and green modulated by -0x1f) every displayed
 * unit whose initial team is not the blue one, skipping units whose ENTD lookup
 * reports -2. */
void script_blue_remove_all_enemy_units(void) {
    s32 i;
    s32 idx;
    s32 out_index;
    unit_stats_t* stats;
    unit_stats_t* found;

    for (i = 0; i < EVENT_UNIT_SLOT_COUNT; i++) {
        if (battle_unit_has_runtime_id(i) != 0) {
            idx = battle_unit_get_battle_id_by_runtime_id(i);
            if (idx != -1) {
                stats = battle_unit_get_stats_from_battle_id(idx);
                found = battle_unit_find_stats_by_entd_unit_id(stats->unit_id, &out_index);
                if ((found->initial_team_flags & BATTLE_TEAM_MASK) && out_index != -2) {
                    battle_gfx_start_event_unit_fade_out(i, 2, -0x1F, -0x1F, 0);
                }
            }
        }
    }
}
