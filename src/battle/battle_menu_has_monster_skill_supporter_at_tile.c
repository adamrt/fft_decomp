#include "fft/battle.h"
#include "fft/unit_slots.h"
#include "psx/types.h"

/* Returns whether a same-team Monster Skill supporter occupies the supplied tile. */
s32 battle_menu_has_monster_skill_supporter_at_tile(s32 team_flags, s32 x, s32 y, s32 total_height) {
    battle_unit_height_data_t height;
    s32 reference_height;
    s32 unit_index;
    s32 unit_team_flags;

    unit_team_flags = team_flags;
    if (x < 0 || x >= g_battle_map_max_x) {
        return 0;
    }
    if (y >= 0) {
        if (y >= g_battle_map_max_y) {
            return 0;
        }

        unit_index = 0;
        reference_height = total_height & 0xff;
        do {
            battle_stats_t* unit = &g_battle_unit_stats[unit_index];

            if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE || unit->x != x || unit->position.bits.y != y
                || battle_action_can_unit_react(unit) != 0
                || !(unit->support_abilities[3] & BATTLE_SUPPORT_SET_4_MONSTER_SKILL)
                || ((unit->team_flags ^ unit_team_flags) & BATTLE_TEAM_MASK)) {
                /* Rejected units continue with the shared scan tail below. */
            } else {
                battle_calculate_unit_height_data(&height, unit->misc_unit_id);
                if ((u32)(reference_height - height.total_height + 5) < 11) {
                    return 1;
                }
            }
            unit_index++;
        } while (unit_index < BATTLE_UNIT_SLOT_COUNT);
    }

    return 0;
}
