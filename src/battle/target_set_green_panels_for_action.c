#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_effect.h"
#include "fft/battle_map.h"
#include "fft/battle_status.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/*
 * Mark the panels an AI action would affect and return how many are
 * targetable.
 *
 * `location` carries the column into the index sum: the extra copies give the
 * index pseudo the allocation priority the target needs (a2), while keeping the
 * `(elevation * 256 + y * width) + x` operand order.
 */
s32 target_set_green_panels_for_action(const action_command_t* action) {
    /* A byte collision test retains the original single-mask branch. */
    action_command_t action_copy;
    ability_secondary_data_t* ability;
    unit_stats_t* unit;
    unit_stats_t* target;
    map_tile_t* tile;
    /* Byte access retains the original packed load and field-extraction order. */
    target_panel_t* panel;
    target_panel_t* other_panel;
    target_panel_t* cursor;
    target_panel_t* marked;
    s32 x;
    s32 y;
    s32 elevation;
    s32 index;
    s32 location;
    s32 height;
    s32 i;
    s32 count;
    u8 unit_id;
    u8 flags_1;
    u8 flags_2;
    u8 aoe;
    u8 vertical;

    main_util_copy_action_data(action, &action_copy);
    ability = &g_main_ability_range_data[action_copy.ability_id];
    flags_1 = ability->flags_1;
    flags_2 = ability->flags_2;
    aoe = ability->aoe;
    vertical = ability->vertical;
    unit_id = action_copy.actor_id;
    unit = &g_battle_unit_stats[unit_id];
    if (ability->flags_4 & ABILITY_SECONDARY_FLAG_4_DIRECT_TARGETING) {
        flags_1 |= ABILITY_SECONDARY_FLAG_1_WEAPON_RANGE;
        flags_2 &= ABILITY_SECONDARY_FLAG_2_CANNOT_HIT_CASTER | ABILITY_SECONDARY_FLAG_2_RANDOM_FIRE
            | ABILITY_SECONDARY_FLAG_2_NO_FOLLOW_TARGET | ABILITY_SECONDARY_FLAG_2_TOP_DOWN;
    }
    if (flags_2 & (ABILITY_SECONDARY_FLAG_2_THREE_DIRECTIONS | ABILITY_SECONDARY_FLAG_2_LINEAR_ATTACK)) {
        x = unit->x;
        y = unit->position.y;
        elevation = unit->position.higher_elevation;
    } else if (action_copy.targeting_type != BATTLE_TARGETING_TYPE_UNIT) {
        x = (u8)action_copy.target_position.vx;
        y = (u8)action_copy.target_position.vz;
        elevation = (u8)action_copy.target_position.vy;
    } else {
        target = &g_battle_unit_stats[action_copy.target_id];
        x = target->x;
        y = target->position.y;
        elevation = target->position.higher_elevation;
    }
    if (x >= g_map_max_x) {
        return -1;
    }
    if (y >= g_map_max_y || elevation >= 2) {
        return -1;
    }
    location = x;
    index = location;
    index = elevation * 256 + y * g_map_max_x + location;
    tile = &g_map_tile_data[index];
    if (tile->flags.blocked) {
        return -1;
    }
    height = tile->height * 2 + ((*(u8*)&tile->depth_half_height) & MAP_TILE_HALF_HEIGHT_MASK)
        + ((*(u8*)&tile->depth_half_height) >> 5) * 2;
    if (aoe == 0xff) {
        cursor = g_target_panels;
        for (i = 0; i < 0x200; i++) {
            cursor[i].remaining_range = 1;
            cursor[i].mark = 0;
        }
    } else {
        cursor = g_target_panels;
        for (i = 0; i < 0x200; i++) {
            cursor[i].remaining_range = 0;
            cursor[i].mark = 0;
        }
        panel = &g_target_panels[index];
        panel->remaining_range = aoe + 1;
        if (index < 0x100) {
            other_panel = &g_target_panels[index + 0x100];
            panel->mark = 1;
        } else {
            other_panel = &g_target_panels[index - 0x100];
            other_panel->mark = 1;
        }
        g_target_panels[index & 0xff].mark = 1;
        if (!(flags_1 & ABILITY_SECONDARY_FLAG_1_WEAPON_RANGE)) {
            panel->remaining_range = aoe + 1;
            other_panel->remaining_range = aoe + 1;
            target_spread_panels(aoe, 0);
            target_apply_vertical_tolerance(height, vertical, (flags_2 & ABILITY_SECONDARY_FLAG_2_TOP_DOWN) != 0);
            target_check_moldball_virus_depth(action_copy.ability_id);
        }
    }
    cursor = &g_target_panels[map_calculate_location(unit)];
    if (flags_2 & ABILITY_SECONDARY_FLAG_2_CANNOT_HIT_CASTER) {
        cursor->remaining_range = 0;
    }
    if (flags_2 & ABILITY_SECONDARY_FLAG_2_LINEAR_ATTACK) {
        target_build_directional_attack_panels(&action_copy, 1);
    }
    if (flags_2 & ABILITY_SECONDARY_FLAG_2_THREE_DIRECTIONS) {
        target_build_directional_attack_panels(&action_copy, 3);
    }
    if (flags_2 & (ABILITY_SECONDARY_FLAG_2_CAN_TARGET_ENEMIES | ABILITY_SECONDARY_FLAG_2_CAN_TARGET_ALLIES)) {
        target_apply_unit_team_eligibility(unit_id, flags_2 & ABILITY_SECONDARY_FLAG_2_CAN_TARGET_ALLIES,
            flags_2 & ABILITY_SECONDARY_FLAG_2_CAN_TARGET_ENEMIES, aoe == 0xff);
    }
    count = 0;
    if (!(flags_2 & (ABILITY_SECONDARY_FLAG_2_CAN_TARGET_ENEMIES | ABILITY_SECONDARY_FLAG_2_CAN_TARGET_ALLIES))) {
        tile = g_map_tile_data;
        for (i = 0, marked = g_target_panels; i < 0x200; i++) {
            if (marked->remaining_range != 0 && !(((*(u8*)&tile[i].flags) & MAP_TILE_COLLISION_MASK))) {
                count++;
                tile[i].ceiling_depth_and_marks.targeted = 1;
            } else {
                tile[i].ceiling_depth_and_marks.targeted = 0;
            }
            marked++;
        }
        return count;
    }
    tile = g_map_tile_data;
    for (i = 0, marked = g_target_panels; i < 0x200; i++) {
        if (marked->mark != 0 && !(((*(u8*)&tile[i].flags) & MAP_TILE_COLLISION_MASK))) {
            count++;
            tile[i].ceiling_depth_and_marks.targeted = 1;
        } else {
            tile[i].ceiling_depth_and_marks.targeted = 0;
        }
        marked++;
    }
    return count;
}
