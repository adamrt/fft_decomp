#include "fft/battle.h"
#include "psx/types.h"

/* Mark every tile the acting unit's Jump abilities can reach.
 *
 * Horizontal range and vertical reach start at 1/2 (8/0x10 for job 0x5d) and
 * grow to the best learned Jump record; panels are spread from the unit's
 * tile, and each map position keeps the higher of its two layers when that
 * layer lies within the vertical window. Returns the number of tiles marked,
 * or 0 when the other layer at the unit's position is taller and passable.
 *
 * The target initializes the lower-layer panel giv first (t1 = panels,
 * t2 = t1 + 0x500). That needs `upper_panels` derived from `panels` and the
 * per-tile panel pointers taken upper layer first, because loop.c keeps its
 * giv list in reverse discovery order. */
s32 battle_target_set_jump_ability_panels(const u8* source) {
    u16 abilities[0x14];
    u8 mp_costs[0x18];
    u8 ability_ct[0x48];
    u8 usable[0x18];
    u8 turns[0x18];
    u8 action[20];
    battle_stats_t* unit;
    map_tile_t* tile;
    map_tile_t* upper;
    map_tile_t* lower;
    battle_target_panel_t* panel;
    battle_target_panel_t* other_panel;
    battle_target_panel_t* panels;
    battle_target_panel_t* upper_panels;
    s32 count;
    s32 i;
    s32 index;
    u8 jump_vertical;
    s32 other_height;
    s32 jump_index;
    u16 ability_id;
    battle_loader_jump_record_t* jump;
    s32 location;
    s32 other_location;
    s16 height;
    u32 tile_height;
    s16 max_height;
    s32 marked;
    s16 min_height;
    s16 upper_height;
    s16 lower_height;
    u8 range;
    u8 vertical;

    range = 1;
    main_util_copy_action_data(source, action);
    unit = &g_battle_unit_stats[action[0]];
    vertical = 2;
    if (unit->job_id == JOB_ID_MIME) {
        range = 8;
        vertical = 0x10;
    }
    location = battle_map_calculate_location(unit);
    tile = &g_battle_map_tile_data[location];
    tile_height = tile->height;
    height = (tile->depth_half_height & MAP_TILE_HALF_HEIGHT_MASK) + tile_height * 2;
    count = battle_menu_get_unit_skillset_ability_data(
        action[0], action[1], (s16*)abilities, mp_costs, ability_ct, 0, usable, turns);
    for (i = 0; i < count; i++) {
        ability_id = abilities[i];
        if ((u16)(ability_id + 0xFE76) < 12) {
            jump_index = ability_id - ABILITY_ID_JUMP_FIRST;
            jump = &g_main_jump_ability_data[(u16)jump_index];
            jump_vertical = jump->vertical * 2;
            if (range < jump->range) {
                range = jump->range;
            }
            if (jump_vertical > vertical) {
                vertical = jump_vertical;
            }
        }
    }
    min_height = height - vertical;
    max_height = height + vertical;
    if (min_height < 0) {
        min_height = 0;
    }
    battle_target_clear_panel_data();
    if (location < 0x100) {
        other_location = location + 0x100;
    } else {
        other_location = location - 0x100;
    }
    panel = &g_battle_target_panels[location];
    other_panel = &g_battle_target_panels[other_location];
    panel->remaining_range = range + 1;
    other_panel->remaining_range = range + 1;
    if (location < 0x100) {
        panel->mark = 1;
    } else {
        other_panel->mark = 1;
    }
    battle_target_spread_panels(range, 0);
    tile = &g_battle_map_tile_data[other_location];
    panel->remaining_range = 0;
    other_panel->remaining_range = 0;
    other_height = (tile->depth_half_height & MAP_TILE_HALF_HEIGHT_MASK) + tile->height * 2;
    if ((tile->flags_06.value & MAP_TILE_FLAG_BLOCKED) == 0 && height < other_height) {
        return 0;
    }
    marked = 0;
    panels = g_battle_target_panels;
    upper_panels = panels + 0x100;
    for (i = 0; i < 0x100; i++) {
        other_panel = &upper_panels[i];
        panel = &panels[i];
        lower = &g_battle_map_tile_data[i];
        upper = &g_battle_map_tile_data[i + 0x100];
        if (panel->remaining_range == 0 || (lower->flags_06.value & MAP_TILE_FLAG_BLOCKED)) {
            lower_height = -1;
        } else {
            lower_height = (lower->depth_half_height & MAP_TILE_HALF_HEIGHT_MASK) + lower->height * 2;
        }
        if (other_panel->remaining_range == 0 || (upper->flags_06.value & MAP_TILE_FLAG_BLOCKED)) {
            upper_height = -1;
        } else {
            upper_height = (upper->depth_half_height & MAP_TILE_HALF_HEIGHT_MASK) + upper->height * 2;
        }
        if (lower_height < upper_height) {
            tile = lower;
            lower_height = upper_height;
            lower = upper;
            upper = tile;
        }
        if (lower_height >= min_height && max_height >= lower_height
            && (lower->flags_06.value & MAP_TILE_FLAG_UNTARGETABLE) == 0) {
            marked++;
            lower->ceiling_depth_and_marks |= MAP_TILE_FLAG_ABILITY_RANGE;
        } else {
            lower->ceiling_depth_and_marks &= 0xBF;
        }
        upper->ceiling_depth_and_marks &= 0xBF;
    }
    return marked;
}
