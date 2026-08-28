#include "fft/battle.h"
#include "fft/data.h"
#include "fft/unit_slots.h"
#include "psx/types.h"

/*
 * Marks the targeting panel under `unit` for each simulation slot.
 * The retail loop never advances the unit pointer, so it re-marks the same
 * panel; the shape is kept as compiled.
 */
void battle_target_set_state_for_all_unit_panels(battle_stats_t* unit, s32 flags) {
    u8* panel;
    s32 i;
    s32 mark_allies;
    s32 mark_enemies;
    s32 team_diff;
    s16 team_diff_copy;
    s32 team;

    mark_allies = flags & ABILITY_SECONDARY_FLAG_2_CAN_TARGET_ALLIES;
    mark_enemies = flags & ABILITY_SECONDARY_FLAG_2_CAN_TARGET_ENEMIES;
    team = unit->team_flags;
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        if (unit->entd_slot != BATTLE_ENTD_SLOT_NONE) {
            team_diff = team ^ unit->initial_team_flags;
            team_diff_copy = team_diff;
            panel = (u8*)g_battle_target_panel_data + battle_map_calculate_location(unit) * 5;
            if (mark_allies != 0 && (team_diff & BATTLE_TEAM_MASK) == 0) {
                panel[1] = 1;
            }
            if (mark_enemies != 0 && (team_diff_copy & BATTLE_TEAM_MASK) != 0) {
                panel[1] = 1;
            }
        }
    }
}
