#include "fft/battle.h"

/*
 * Mark the panels an AI action would affect and return how many are
 * targetable.
 *
 * `location` carries the column into the index sum: the extra copies give the
 * index pseudo the allocation priority the target needs (a2), while keeping the
 * `(elevation * 256 + y * width) + x` operand order.
 */
s32 battle_target_set_green_panels_for_action(battle_ai_command_action_t* action) {
    battle_ai_command_action_t action_copy;
    ability_secondary_data_t* ability;
    battle_stats_t* unit;
    battle_stats_t* target;
    map_tile_t* tile;
    battle_target_panel_t* panel;
    battle_target_panel_t* other_panel;
    battle_target_panel_t* cursor;
    battle_target_panel_t* marked;
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

    main_util_copy_action_data((const u8*)action, (u8*)&action_copy);
    ability = &g_main_ability_range_data[(s16)action_copy.ability_id];
    flags_1 = ability->flags_1;
    flags_2 = ability->flags_2;
    aoe = ability->aoe;
    vertical = ability->vertical;
    unit_id = action_copy.unit_id;
    unit = &g_battle_unit_stats[unit_id];
    if (ability->flags_4 & 0x20) {
        flags_1 |= 0x20;
        flags_2 &= 0x39;
    }
    if (flags_2 & 6) {
        x = unit->x;
        y = unit->position.bits.y;
        elevation = unit->position.bits.higher_elevation;
    } else if (action_copy.targeting_type != 6) {
        x = (u8)action_copy.target_x;
        y = (u8)action_copy.target_y;
        elevation = (u8)action_copy.target_elevation;
    } else {
        target = &g_battle_unit_stats[action_copy.target_id];
        x = target->x;
        y = target->position.bits.y;
        elevation = target->position.bits.higher_elevation;
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
    tile = &g_battle_map_tile_data[index];
    if (tile->flags_06.value & 1) {
        return -1;
    }
    height = tile->height * 2 + (tile->depth_half_height & 0x1f) + (tile->depth_half_height >> 5) * 2;
    if (aoe == 0xff) {
        cursor = g_battle_target_panels;
        for (i = 0; i < 0x200; i++) {
            cursor[i].remaining_range = 1;
            cursor[i].mark = 0;
        }
    } else {
        cursor = g_battle_target_panels;
        for (i = 0; i < 0x200; i++) {
            cursor[i].remaining_range = 0;
            cursor[i].mark = 0;
        }
        panel = &g_battle_target_panels[index];
        panel->remaining_range = aoe + 1;
        if (index < 0x100) {
            other_panel = &g_battle_target_panels[index + 0x100];
            panel->mark = 1;
        } else {
            other_panel = &g_battle_target_panels[index - 0x100];
            other_panel->mark = 1;
        }
        g_battle_target_panels[index & 0xff].mark = 1;
        if (!(flags_1 & 0x20)) {
            panel->remaining_range = aoe + 1;
            other_panel->remaining_range = aoe + 1;
            battle_target_spread_panels(aoe, 0);
            battle_target_apply_vertical_tolerance(height, vertical, (flags_2 >> 5) & 1);
            battle_target_check_moldball_virus_depth(action_copy.ability_id);
        }
    }
    cursor = &g_battle_target_panels[battle_map_calculate_location(unit)];
    if (flags_2 & 1) {
        cursor->remaining_range = 0;
    }
    if (flags_2 & 4) {
        battle_target_build_directional_attack_panels(&action_copy, 1);
    }
    if (flags_2 & 2) {
        battle_target_build_directional_attack_panels(&action_copy, 3);
    }
    if (flags_2 & 0xc0) {
        battle_target_apply_unit_team_eligibility(unit_id, flags_2 & 0x80, flags_2 & 0x40, aoe == 0xff);
    }
    count = 0;
    if (!(flags_2 & 0xc0)) {
        tile = g_battle_map_tile_data;
        for (i = 0, marked = g_battle_target_panels; i < 0x200; i++) {
            if (marked->remaining_range != 0 && !(tile[i].flags_06.value & MAP_TILE_COLLISION_MASK)) {
                count++;
                tile[i].ceiling_depth_and_marks |= MAP_TILE_FLAG_TARGETED;
            } else {
                tile[i].ceiling_depth_and_marks &= ~MAP_TILE_FLAG_TARGETED;
            }
            marked++;
        }
        return count;
    }
    tile = g_battle_map_tile_data;
    for (i = 0, marked = g_battle_target_panels; i < 0x200; i++) {
        if (marked->mark != 0 && !(tile[i].flags_06.value & MAP_TILE_COLLISION_MASK)) {
            count++;
            tile[i].ceiling_depth_and_marks |= MAP_TILE_FLAG_TARGETED;
        } else {
            tile[i].ceiling_depth_and_marks &= ~MAP_TILE_FLAG_TARGETED;
        }
        marked++;
    }
    return count;
}
