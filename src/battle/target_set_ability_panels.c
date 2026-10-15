#include "fft/battle.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/*
 * Seed the targeting panels for the action in `source` and apply the ability's
 * range, vertical tolerance and targeting flags. Returns the number of tiles
 * the final pass marks as in ability range.
 *
 * The separate byte offset and register binding preserve the target's copy
 * of the location index. Direct typed indexing removes that copy and changes
 * the later register allocation; the binding emits no instructions.
 */
s32 target_set_ability_panels(const action_command_t* source) {
    action_command_t action;
    unit_stats_t* unit;
    ability_secondary_data_t* ability;
    target_panel_t* panels;
    target_panel_t* origin;
    target_panel_t* seed;
    target_panel_t* seed_upper;
    map_tile_t* tile;
    map_tile_depth_t terrain_depth;
    s32 location;
    s32 index;
    s32 elevation;
    /* Pin: unpinned, GCC computes the origin offset in $v0 instead of $v1. */
    register s32 origin_offset __asm__("$3");
    s32 i;
    u8 range;
    u8 aoe;
    u8 vertical;
    u8 flags_1;
    u8 flags_4;
    s32 x;
    u8 y;

    main_util_copy_action_data(source, &action);
    unit = &g_battle_unit_stats[action.actor_id];
    ability = &g_main_ability_range_data[action.ability_id];
    range = ability->range;
    aoe = ability->aoe;
    flags_1 = ability->flags_1;
    vertical = ability->vertical;
    flags_4 = ability->flags_4;
    x = unit->x;
    y = unit->position.y;
    location = map_calculate_location(unit);
    tile = &g_map_tile_data[location];
    terrain_depth = tile->depth_half_height;
    elevation = tile->height * 2 + (terrain_depth.half_height) + tile->depth_half_height.depth * 2;
    panels = g_target_panels;
    origin_offset = location * sizeof(target_panel_t);
    origin = (target_panel_t*)((s32)panels + origin_offset);
    index = y * g_map_max_x + x;
    if (range == 0xff) {
        for (i = 0; i < 0x200; i++) {
            panels[i].remaining_range = 1;
            panels[i].mark = 0;
        }
    } else {
        for (i = 0; i < 0x200; i++) {
            panels[i].remaining_range = 0;
            panels[i].mark = 0;
        }
        seed = &g_target_panels[index];
        seed_upper = &g_target_panels[index + 0x100];
        seed->remaining_range = range + 1;
        seed_upper->remaining_range = range + 1;
        seed->mark = 1;
        if (flags_1 & ABILITY_SECONDARY_FLAG_1_WEAPON_RANGE) {
            target_calculate_weapon_range(unit);
            seed->remaining_range = 0;
            seed_upper->remaining_range = 0;
        } else {
            target_spread_panels(range, 0);
        }
    }
    if (flags_1 & ABILITY_SECONDARY_FLAG_1_CANNOT_TARGET_SELF) {
        origin->remaining_range = 0;
    }
    if (flags_1 & ABILITY_SECONDARY_FLAG_1_VERTICAL_FIXED) {
        location = x;
        target_apply_vertical_fixed(location, y);
    }
    if (flags_1 & ABILITY_SECONDARY_FLAG_1_VERTICAL_TOLERANCE) {
        target_apply_vertical_tolerance(elevation, vertical, 0);
    }
    if (aoe == 0 || (flags_4 & ABILITY_SECONDARY_FLAG_4_DIRECT_TARGETING)) {
        target_clear_panels_on_untargetable_tiles();
    }
    if (flags_1 & (ABILITY_SECONDARY_FLAG_1_ENEMY_UNIT_PANELS | ABILITY_SECONDARY_FLAG_1_ALLY_UNIT_PANELS)) {
        target_set_state_for_all_unit_panels(unit, flags_1);
    }
    if (flags_1
        & (ABILITY_SECONDARY_FLAG_1_VERTICAL_FIXED | ABILITY_SECONDARY_FLAG_1_ENEMY_UNIT_PANELS
            | ABILITY_SECONDARY_FLAG_1_ALLY_UNIT_PANELS)) {
        return target_clear_selection_state_of_all_panels();
    }
    return target_set_all_panels_targeted_if_targetable();
}
