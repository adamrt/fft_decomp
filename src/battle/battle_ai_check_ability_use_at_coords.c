#include "fft/battle.h"

/*
 * Check depth, terrain, and Monster Skill restrictions at an AI origin.
 *
 * Return 1 when these checks permit use, otherwise 0. Geomancy delegates
 * directly to its terrain check; this is not a complete ability-use test.
 */
s32 battle_ai_check_ability_use_at_coords(battle_ai_coords_t* coords) {
    battle_ai_data_t* ai = &g_battle_ai_data_base;
    s32 i;
    s32 dx;
    s32 dy;
    battle_stats_t* unit;

    if (g_battle_ai_data_base.water_penalty) {
        s32 depth = g_battle_map_tile_data[coords->bytes.y * g_battle_ai_data_base.map_max_x + coords->bytes.x
                        + (coords->bytes.elevation << 8)]
                        .depth_half_height
            >> MAP_TILE_DEPTH_SHIFT;
        if (depth >= 2)
            return 0;
    }
    if (g_main_action_menu_types_by_skillset[ai->considered_ability.skillset] == ACTION_MENU_TYPE_ELEMENTS)
        return battle_ai_can_elemental_ability_be_used(coords);
    if (ai->considered_ability.ai_flags.word & BATTLE_AI_ABILITY_REQUIRES_MONSTER_SKILL) {
        for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
            unit = &g_battle_unit_stats[i];
            if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE)
                continue;
            if (!(unit->support_abilities[3] & BATTLE_SUPPORT_SET_4_MONSTER_SKILL))
                continue;
            if ((unit->initial_team_flags & BATTLE_TEAM_MASK) != ai->acting_unit_team)
                continue;
            if (main_unit_has_status_in_set(unit, MAIN_STATUS_CHECK_SET_PREVENT_REACTION))
                continue;
            dx = unit->x - coords->bytes.x;
            if (dx < 0)
                dx = -dx;
            dy = unit->position.bits.y - coords->bytes.y;
            if (dy < 0)
                dy = -dy;
            if (dx + dy < 2) {
                if ((u32)(battle_ai_calculate_height_difference_between_units(coords, unit) + 2) < 5)
                    return 1;
            }
        }
        return 0;
    }
    return 1;
}
