#include "fft/battle_action.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* Returns whether a same-team Monster Skill supporter occupies the supplied tile. */
s32 battle_menu_has_monster_skill_supporter_at_tile(s32 team_flags, s32 x, s32 y, s32 total_height) {
    battle_unit_height_data_t height;
    u8 reference_height;
    s32 unit_index;
    s32 unit_team_flags;

    unit_team_flags = team_flags;
    if (x < 0 || x >= g_map_max_x) {
        return 0;
    }
    if (y >= 0) {
        if (y >= g_map_max_y) {
            return 0;
        }

        unit_index = 0;
        reference_height = total_height;
        for (; unit_index < BATTLE_UNIT_SLOT_COUNT; unit_index++) {
            unit_stats_t* unit = &g_battle_unit_stats[unit_index];

            if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE || unit->x != x || unit->position.y != y
                || action_is_reaction_prevented(unit) != 0
                || !(unit->ability_flags.support[3] & BATTLE_SUPPORT_SET_4_MONSTER_SKILL)
                || ((unit->team_flags ^ unit_team_flags) & BATTLE_TEAM_MASK)) {
                continue;
            }
            battle_unit_calculate_height_data(&height, unit->battle_id);
            if ((reference_height - height.total_height) >= -5 && (reference_height - height.total_height) <= 5) {
                return 1;
            }
        }
    }

    return 0;
}
