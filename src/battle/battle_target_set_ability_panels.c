#include "fft/battle.h"
#include "psx/types.h"

/*
 * Seed the targeting panels for the action in `source` and apply the ability's
 * range, vertical tolerance and targeting flags.
 *
 * The separate byte offset and register binding preserve the target's copy
 * of the location index. Direct typed indexing removes that copy and changes
 * the later register allocation; the binding emits no instructions.
 */
void battle_target_set_ability_panels(const u8* source) {
    battle_ai_command_action_t action;
    battle_stats_t* unit;
    ability_secondary_data_t* ability;
    battle_target_panel_t* panels;
    battle_target_panel_t* origin;
    battle_target_panel_t* seed;
    battle_target_panel_t* seed_upper;
    map_tile_t* tile;
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

    main_util_copy_action_data(source, (u8*)&action);
    unit = &g_battle_unit_stats[action.unit_id];
    ability = &g_main_ability_range_data[(s16)action.ability_id];
    range = ability->range;
    aoe = ability->aoe;
    flags_1 = ability->flags_1;
    vertical = ability->vertical;
    flags_4 = ability->flags_4;
    x = unit->x;
    y = unit->position.bits.y;
    location = battle_map_calculate_location(unit);
    tile = &g_battle_map_tile_data[location];
    elevation = tile->height * 2 + (tile->depth_half_height & 0x1f) + (tile->depth_half_height >> 5) * 2;
    panels = g_battle_target_panels;
    origin_offset = location * sizeof(battle_target_panel_t);
    origin = (battle_target_panel_t*)((s32)panels + origin_offset);
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
        seed = &g_battle_target_panels[index];
        seed_upper = &g_battle_target_panels[index + 0x100];
        seed->remaining_range = range + 1;
        seed_upper->remaining_range = range + 1;
        seed->mark = 1;
        if (flags_1 & ABILITY_SECONDARY_FLAG_1_WEAPON_RANGE) {
            battle_target_calculate_weapon_range(unit);
            seed->remaining_range = 0;
            seed_upper->remaining_range = 0;
        } else {
            battle_target_spread_panels(range, 0);
        }
    }
    if (flags_1 & ABILITY_SECONDARY_FLAG_1_CANNOT_TARGET_SELF) {
        origin->remaining_range = 0;
    }
    if (flags_1 & 0x10) {
        location = x;
        battle_target_apply_vertical_fixed(location, y);
    }
    if (flags_1 & 0x08) {
        battle_target_apply_vertical_tolerance(elevation, vertical, 0);
    }
    if (aoe == 0 || (flags_4 & 0x20)) {
        battle_target_clear_panels_on_untargetable_tiles();
    }
    if (flags_1 & 0xc0) {
        battle_target_set_state_for_all_unit_panels(unit, flags_1);
    }
    if (!(flags_1 & 0xd0)) {
        battle_target_set_all_panels_targeted_if_targetable();
        return;
    }
    battle_target_clear_selection_state_of_all_panels();
}
