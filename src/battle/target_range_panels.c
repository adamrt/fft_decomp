/* Targeting panel spread, weapon/ability ranges and publication. */

#include "fft/battle.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* For Moldball Virus, clear the targeting panel of every tile with depth. */
void target_check_moldball_virus_depth(s16 ability_id) {
    map_tile_t* tile;
    target_panel_t* panel;
    s32 i;

    if (ability_id != ABILITY_ID_MONSTER_GREAT_MORBOL_MOLDBALL_VIRUS)
        return;
    i = 0;
    panel = g_target_panel_data;
    tile = g_map_tile_data;
    do {
        if (tile->depth_half_height.depth) {
            panel->remaining_range = 0;
        }
        tile++;
        i += 1;
        panel++;
    } while (i < 0x200);
}

/* Clear the range and the mark of each panel whose tile is flagged untargetable.
 * Only the 256 lower-level panels are visited. */
void target_clear_panels_on_untargetable_tiles(void) {
    s32 i;
    map_tile_t* src;
    target_panel_t* dst;

    i = 0;
    dst = g_target_panel_data;
    src = g_map_tile_data;
    for (; i < 256; src++, i++, dst++) {
        if (src->flags.untargetable) {
            dst->remaining_range = 0;
            dst->mark = 0;
        }
    }
}

/* Keeps, per map position, only the terrain layer nearest a reference height.
 *
 * Heights are in half units (height * 2 + slope half-height + depth * 2). A
 * blocked tile clears its panel and counts as height -255. With single_layer
 * set, only the higher layer is considered. The nearer layer survives when it
 * lies within tolerance * 2 of the reference; the other panel is cleared.
 *
 * The upper layer index is a separate variable (j): writing [i + 0x100] lets
 * the front end fold the 0x100 into the symbol, and the target instead
 * strength-reduces 5 * j and 8 * j against hoisted table bases. Block-scoped
 * depth temporaries keep that value out of global allocation. */
void target_apply_vertical_tolerance(u8 ref_height, u8 tolerance, s32 single_layer) {
    s32 max_delta;
    s32 ref;
    s32 i;
    s32 j;
    s32 lower;
    s32 upper;
    s32 lower_delta;
    s32 upper_delta;
    map_tile_t* tile;
    target_panel_t* lower_panel;
    target_panel_t* upper_panel;

    max_delta = tolerance * 2;
    ref = ref_height;
    for (i = 0; i < 0x100; i++) {
        tile = &g_map_tile_data[i];
        lower_panel = &g_target_panel_data[i];
        if (!tile->flags.blocked) {
            map_tile_depth_t depth = tile->depth_half_height;
            lower = tile->height * 2 + depth.half_height + depth.depth * 2;
        } else {
            lower = -0xFF;
            lower_panel->remaining_range = 0;
            lower_panel->mark = 0;
        }
        j = i + 0x100;
        tile = &g_map_tile_data[j];
        upper_panel = &g_target_panel_data[j];
        if (!tile->flags.blocked) {
            map_tile_depth_t depth = tile->depth_half_height;
            upper = tile->height * 2 + depth.half_height + depth.depth * 2;
        } else {
            upper = -0xFF;
            upper_panel->remaining_range = 0;
            upper_panel->mark = 0;
        }
        if (single_layer != 0) {
            if (lower >= upper) {
                upper = -0xFF;
            } else {
                lower = -0xFF;
            }
        }
        if (lower >= ref) {
            lower_delta = lower - ref;
        } else {
            lower_delta = ref - lower;
        }
        if (upper >= ref) {
            upper_delta = upper - ref;
        } else {
            upper_delta = ref - upper;
        }
        if (upper_delta < lower_delta) {
            lower_panel->remaining_range = 0;
            lower_panel->mark = 0;
            if (max_delta < upper_delta) {
                upper_panel->remaining_range = 0;
                upper_panel->mark = 0;
            }
        } else {
            upper_panel->remaining_range = 0;
            upper_panel->mark = 0;
            if (max_delta < lower_delta) {
                lower_panel->remaining_range = 0;
                lower_panel->mark = 0;
            }
        }
    }
}

/* Clear the mark byte (the spread frontier) of all 512 targeting panels. */
void target_clear_panel_spread_flags(void) {
    s32 i;
    target_panel_t* panel;

    for (i = 511, panel = &g_target_panels[511]; i >= 0; i--, panel--) {
        panel->mark = 0;
    }
}

/* Remove the range from every panel, on both levels, within `range` tiles
 * (Manhattan distance) of (x, y). */
void target_remove_close_range(s32 x, s32 y, s32 range) {
    s32 i;
    s32 j;
    s32 dy;
    s32 dx;
    s32 k;

    for (i = 0; i < g_map_max_y; i++) {
        dy = (y < i) ? (i - y) : (y - i);
        for (j = 0; j < g_map_max_x; j++) {
            dx = (x < j) ? (j - x) : (x - j);
            if (range >= (dy + dx)) {
                g_target_panel_data[(i * g_map_max_x) + j].remaining_range = 0;
                k = j + 0x100;
                g_target_panel_data[(i * g_map_max_x) + k].remaining_range = 0;
            }
        }
    }
}

/* Repeat the targeting-panel spread pass up to the given count.
 *
 * Each pass visits every panel whose mark byte flags it as a new frontier,
 * clears the mark and spreads from it; passes stop once none spread. A zero
 * count clears the tile data instead. Every caller passes 0 as the unused
 * second argument. */
void target_spread_panels(u8 passes, s32 unused) {
    s32 y;
    s32 x;
    s32 changed;
    s32 pass;
    target_panel_t* panel;

    if (passes == 0) {
        target_clear_panel_spread_flags();
        return;
    }
    changed = 1;
    for (pass = 0; pass < passes; pass++) {
        if (changed == 0) {
            break;
        }
        changed = 0;
        for (y = 0; y < g_map_max_y; y++) {
            for (x = 0; x < g_map_max_x; x++) {
                panel = &g_target_panels[y * g_map_max_x + x];
                if (panel->mark != 0) {
                    panel->mark = 0;
                    changed += target_spread_panel_to_neighbors(y, x);
                }
            }
        }
    }
}

/* Spread one targeting panel's remaining range to its four orthogonal
 * neighbors. A neighbor whose residual budget is lower than this panel's is
 * refreshed to one less, on both map levels, and marked as a new frontier when
 * enough budget is left to keep spreading. Returns 1 when any neighbor was
 * marked. */
s32 target_spread_panel_to_neighbors(s32 y, s32 x) {
    target_panel_t* panel;
    s32 changed;
    s32 i;
    s32 dx;
    s32 dy;
    s32 next_x;
    s32 next_y;
    s32 index;
    s32 upper_x;
    s32 range;
    s32 spread_range;

    changed = 0;
    range = g_target_panels[y * g_map_max_x + x].remaining_range;
    spread_range = range - 1;
    for (i = 0; i < 4; i++) {
        switch (i) {
        case 0:
            dx = 1;
            dy = 0;
            break;
        case 1:
            dx = 0;
            dy = 1;
            break;
        case 2:
            dx = -1;
            dy = 0;
            break;
        case 3:
            dx = 0;
            dy = -1;
            break;
        }
        next_x = x + dx;
        next_y = y + dy;
        if ((next_x >= 0) && (next_y >= 0) && (next_x < g_map_max_x) && (next_y < g_map_max_y)) {
            index = next_y * g_map_max_x + next_x;
            panel = &g_target_panels[index];
            if (panel->remaining_range < range) {
                panel->remaining_range = spread_range;
                upper_x = next_x + 0x100;
                g_target_panels[next_y * g_map_max_x + upper_x].remaining_range = spread_range;
                if (range >= 3) {
                    panel->mark = 1;
                    changed = 1;
                }
            }
        }
    }
    return changed;
}

/* Compute unit height after mount, Float, and water movement adjustments.
 *
 * Float and BATTLE_MOVEMENT_SET_3_MOVE_IN_WATER stand on the water surface;
 * BATTLE_MOVEMENT_SET_3_WALK_ON_WATER stands one level below it. */
u8 battle_unit_get_effective_height(const unit_stats_t* unit) {
    map_tile_t* tile;
    map_tile_depth_t terrain_depth;
    s32 height;
    u8 effective_height;
    u8 water_depth;
    u8 ignores_water;
    u8 flags;

    ignores_water = 0;
    tile = &g_map_tile_data[map_calculate_location(unit)];
    terrain_depth = tile->depth_half_height;
    flags = unit->mount_info;
    height = tile->height * 2 + (terrain_depth.half_height);
    water_depth = tile->depth_half_height.depth * 2;
    effective_height = height;
    if (flags & BATTLE_MOUNT_INFO_FLAG_RIDER) {
        unit = &g_battle_unit_stats[flags & 0x1f];
        height += 2;
        effective_height = height;
    }
    flags = unit->ability_flags.movement[2];
    if ((flags & BATTLE_MOVEMENT_SET_3_FLOAT)
        || (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FLOAT)] & STATUS_BYTE_MASK(STATUS_ID_FLOAT))) {
        effective_height += 2;
        ignores_water = 1;
    }
    if (water_depth != 0) {
        if (ignores_water || (flags & BATTLE_MOVEMENT_SET_3_MOVE_IN_WATER)) {
            effective_height += water_depth;
        } else if (flags & BATTLE_MOVEMENT_SET_3_WALK_ON_WATER) {
            effective_height += water_depth - 2;
        }
    }
    return effective_height;
}

/*
 * Mark the longbow (arc) targeting panels: every tile at least three steps
 * away whose distance, less a quarter of the unit's height advantage, is
 * within range. Both elevation levels of each tile are tested.
 */
void target_calculate_arc_range(const unit_stats_t* unit, u8 range, u8 flags) {
    u8 height;
    u8 tile_height;
    s32 ux;
    s32 uy;
    s32 x;
    s32 y;
    s32 dy;
    s32 dist;
    s32 level;
    s32 index;
    map_tile_t* tile;

    height = battle_unit_get_effective_height(unit);
    ux = unit->x;
    uy = unit->position.y;
    for (y = 0; y < g_map_max_y; y++) {
        if (uy < y) {
            dy = y - uy;
        } else {
            dy = uy - y;
        }
        for (x = 0; x < g_map_max_x; x++) {
            if (ux < x) {
                dist = x - ux + dy;
            } else {
                dist = ux - x + dy;
            }
            if (dist >= 3) {
                for (level = 0; level < 2; level++) {
                    index = (level << 8) + y * g_map_max_x + x;
                    tile = &g_map_tile_data[index];
                    tile_height
                        = tile->height * 2 + tile->depth_half_height.half_height + tile->depth_half_height.depth * 2;
                    if (dist - (height - tile_height) / 4 <= range) {
                        g_target_panels[index].remaining_range = 1;
                    }
                }
            }
        }
    }
}

/*
 * Mark the targeting panels in range of the unit's weapon.
 *
 * Frog uses the bare-hand entry; an empty right hand falls back to the left
 * hand, and non-weapon items also use entry 0. Striking, lunging, and direct
 * weapon flags select the special range shapes; direct also removes the close
 * range.
 */
void target_calculate_weapon_range(const unit_stats_t* unit) {
    s32 y;
    s32 x;
    s32 index;
    u8 weapon;
    u8 range;
    u8 flags;
    weapon_data_t* data;
    target_panel_t* panel;
    target_panel_t* upper;

    y = unit->position.y;
    weapon = unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON];
    x = unit->x;
    index = x + y * g_map_max_x;
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FROG)] & STATUS_BYTE_MASK(STATUS_ID_FROG)) {
        weapon = ITEM_ID_NOTHING;
    }
    if (weapon == ITEM_ID_NONE) {
        weapon = unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON];
        if (weapon == ITEM_ID_NONE) {
            weapon = ITEM_ID_NOTHING;
        }
    }
    if (weapon >= ITEM_ID_SHIELD_FIRST) {
        weapon = ITEM_ID_NOTHING;
    }
    data = &g_main_item_weapon_data[weapon];
    range = data->range;
    flags = data->flags;
    panel = &g_target_panels[index];
    upper = &g_target_panels[index + 0x100];
    panel->remaining_range = range + 1;
    upper->remaining_range = range + 1;
    panel->mark = 1;
    if (flags & (WEAPON_FLAG_STRIKING | WEAPON_FLAG_LUNGING)) {
        target_calculate_strike_lunge_range(unit, flags);
    } else if (flags & WEAPON_FLAG_DIRECT) {
        target_spread_panels(range, 0);
        target_remove_close_range(x, y, 2);
    } else {
        target_calculate_arc_range(unit, range, flags);
    }
    target_clear_range_on_untargetable_tiles();
}

/*
 * Mark the special weapon-range shapes selected by weapon flags 0xc0: flag
 * 0x80 covers the four adjacent tiles within a narrow height band, otherwise
 * the two rings at distance 1 and 2 use a band that widens with distance.
 */
void target_calculate_strike_lunge_range(const unit_stats_t* unit, u8 flags) {
    u8 height;
    s32 x;
    s32 y;
    s32 i;
    s32 lo;
    s32 hi;

    height = battle_unit_get_effective_height(unit);
    x = unit->x;
    y = unit->position.y;
    if (flags & 0x80) {
        lo = height - 6;
        hi = height + 5;
        target_calculate_aoe_vertical_tolerance(x - 1, y, lo, hi);
        target_calculate_aoe_vertical_tolerance(x + 1, y, lo, hi);
        target_calculate_aoe_vertical_tolerance(x, y - 1, lo, hi);
        target_calculate_aoe_vertical_tolerance(x, y + 1, lo, hi);
    } else {
        for (i = 1; i < 3; i++) {
            lo = height + (i - 5) * 2;
            hi = height + 7;
            target_calculate_aoe_vertical_tolerance(x - i, y, lo, hi);
            target_calculate_aoe_vertical_tolerance(x + i, y, lo, hi);
            target_calculate_aoe_vertical_tolerance(x, y - i, lo, hi);
            target_calculate_aoe_vertical_tolerance(x, y + i, lo, hi);
        }
    }
}

/* Give range 1 to the panels of map position (x, y), on either level, whose
 * height lies in lo..hi. Heights are in half steps (height * 2 + slope half
 * height + depth * 2); a negative lo counts as 0 and a position off the map is
 * ignored. The strike and lunge weapon ranges are built from it. */
void target_calculate_aoe_vertical_tolerance(s32 x, s32 y, s32 lo, s32 hi) {
    s32 level;
    s32 idx;
    s32 height;
    map_tile_t* tile;
    target_panel_t* panel;
    map_tile_depth_t flags;

    if ((x >= 0) && (x < g_map_max_x) && (y >= 0) && (y < g_map_max_y)) {
        level = 0;
        if (lo < 0) {
            lo = 0;
        }
        do {
            idx = (level << 8) + (y * g_map_max_x) + x;
            tile = &g_map_tile_data[idx];
            panel = &g_target_panel_data[idx];
            flags = tile->depth_half_height;
            height = (tile->height * 2) + (flags.half_height) + ((flags.depth) * 2);
            level++;
            if ((height >= lo) && (hi >= height)) {
                panel->remaining_range = 1;
            }
        } while (level < 2);
    }
}

/* Remove the range from every panel, on both levels, whose tile is flagged
 * untargetable. */
void target_clear_range_on_untargetable_tiles(void) {
    s32 i = 0;
    target_panel_t* dst = g_target_panel_data;
    map_tile_t* src = g_map_tile_data;
    for (; i < 0x200; i++) {
        if (src->flags.untargetable)
            dst->remaining_range = 0;
        src++;
        dst++;
    }
}

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
s32 target_set_jump_ability_panels(const action_command_t* source) {
    u16 abilities[0x14];
    u8 mp_costs[0x18];
    u8 ability_ct[0x48];
    u8 usable[0x18];
    u8 turns[0x18];
    action_command_t action;
    unit_stats_t* unit;
    map_tile_t* tile;
    map_tile_t* upper;
    map_tile_t* lower;
    target_panel_t* panel;
    target_panel_t* other_panel;
    target_panel_t* panels;
    target_panel_t* upper_panels;
    s32 count;
    s32 i;
    u8 jump_vertical;
    s32 other_height;
    s32 jump_index;
    u16 ability_id;
    main_ability_jump_record_t* jump;
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
    main_util_copy_action_data(source, &action);
    unit = &g_battle_unit_stats[action.actor_id];
    vertical = 2;
    if (unit->job_id == JOB_ID_MIME) {
        range = 8;
        vertical = 0x10;
    }
    location = map_calculate_location(unit);
    tile = &g_map_tile_data[location];
    tile_height = tile->height;
    height = tile->depth_half_height.half_height + tile_height * 2;
    count = battle_menu_get_unit_skillset_ability_data(
        action.actor_id, action.skillset_id, (s16*)abilities, mp_costs, ability_ct, 0, usable, turns);
    for (i = 0; i < count; i++) {
        ability_id = abilities[i];
        if (ability_id >= ABILITY_ID_JUMP_FIRST && ability_id < ABILITY_ID_CHARGE_FIRST) {
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
    target_clear_panel_data();
    if (location < 0x100) {
        other_location = location + 0x100;
    } else {
        other_location = location - 0x100;
    }
    panel = &g_target_panels[location];
    other_panel = &g_target_panels[other_location];
    panel->remaining_range = range + 1;
    other_panel->remaining_range = range + 1;
    if (location < 0x100) {
        panel->mark = 1;
    } else {
        other_panel->mark = 1;
    }
    target_spread_panels(range, 0);
    tile = &g_map_tile_data[other_location];
    panel->remaining_range = 0;
    other_panel->remaining_range = 0;
    other_height = tile->depth_half_height.half_height + tile->height * 2;
    if ((tile->flags.blocked) == 0 && height < other_height) {
        return 0;
    }
    marked = 0;
    panels = g_target_panels;
    upper_panels = panels + 0x100;
    for (i = 0; i < 0x100; i++) {
        other_panel = &upper_panels[i];
        panel = &panels[i];
        lower = &g_map_tile_data[i];
        upper = &g_map_tile_data[i + 0x100];
        if (panel->remaining_range == 0 || (lower->flags.blocked)) {
            lower_height = -1;
        } else {
            lower_height = lower->depth_half_height.half_height + lower->height * 2;
        }
        if (other_panel->remaining_range == 0 || (upper->flags.blocked)) {
            upper_height = -1;
        } else {
            upper_height = upper->depth_half_height.half_height + upper->height * 2;
        }
        if (lower_height < upper_height) {
            tile = lower;
            lower_height = upper_height;
            lower = upper;
            upper = tile;
        }
        if (lower_height >= min_height && max_height >= lower_height && (lower->flags.untargetable) == 0) {
            marked++;
            lower->ceiling_depth_and_marks.ability_range = 1;
        } else {
            lower->ceiling_depth_and_marks.ability_range = 0;
        }
        upper->ceiling_depth_and_marks.ability_range = 0;
    }
    return marked;
}

/* Spread an ability range from the unit's tile and mark the reachable panels.
 *
 * Range bits 0-6 seed the unit's panel on both map levels and bit 7 excludes
 * the unit's own tile afterwards. Panels flagged in flags bits 0-1 are never
 * marked. Returns the number of marked panels. */
s32 target_calculate_ability_range_with_map_parameters(const unit_stats_t* unit, u8 range) {
    /* A byte collision test retains the original single-mask branch. */
    target_panel_t* panel;
    target_panel_t* upper;
    target_panel_t* panels;
    map_tile_t* tile;
    s32 index;
    s32 i;
    s32 count;
    u8 remaining;

    index = unit->position.y * g_map_max_x + unit->x;
    target_clear_panel_data();
    remaining = (range & 0x7f) + 1;
    panel = &g_target_panels[index];
    upper = &g_target_panels[index + 0x100];
    panel->remaining_range = remaining;
    upper->remaining_range = remaining;
    panel->mark = 1;
    target_spread_panels(range, 0);
    if (range & 0x80) {
        panel->remaining_range = 0;
        upper->remaining_range = 0;
    }
    count = 0;
    tile = g_map_tile_data;
    for (i = 0, panels = g_target_panels; i < 0x200; i++) {
        if (panels->remaining_range != 0 && !(((*(u8*)&tile[i].flags) & MAP_TILE_COLLISION_MASK))) {
            tile[i].ceiling_depth_and_marks.ability_range = 1;
            count++;
        } else {
            tile[i].ceiling_depth_and_marks.ability_range = 0;
        }
        panels++;
    }
    return count;
}

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

/* Build the targeting panels for a weapon attack: the weapon's range around the
 * acting unit, without the unit's own tile. Returns the number of tiles in range. */
s32 target_set_weapon_attack_panels(const action_command_t* source) {
    action_command_t action;
    unit_stats_t* unit;
    s32 idx;

    main_util_copy_action_data(source, &action);
    unit = &g_battle_unit_stats[action.actor_id];
    idx = unit->position.y * g_map_max_x + unit->x;
    target_clear_panel_data();
    target_calculate_weapon_range(unit);
    {
        target_panel_t* p1;
        target_panel_t* p2;
        p1 = &g_target_panel_data[idx];
        p2 = &g_target_panel_data[idx + 0x100];
        p1->remaining_range = 0;
        p2->remaining_range = 0;
        return target_set_all_panels_targeted_if_targetable();
    }
}

/* Publish the panels in range to the map: a tile whose panel has range left gets
 * MAP_TILE_FLAG_ABILITY_RANGE unless it is blocked or a cross-section surface,
 * and every other tile loses the flag. Returns the number of flagged tiles. */
s32 target_set_all_panels_targeted_if_targetable(void) {
    map_tile_t* tile;
    target_panel_t* panel;
    s32 count;
    s32 i;

    count = 0;
    for (i = 0; i < 0x200; i++) {
        tile = &g_map_tile_data[i];
        panel = &g_target_panel_data[i];
        if (panel->remaining_range != 0 && !(tile->flags.blocked)
            && (tile->surface.type) != MAP_SURFACE_CROSS_SECTION) {
            count++;
            tile->ceiling_depth_and_marks.ability_range = 1;
        } else {
            tile->ceiling_depth_and_marks.ability_range = 0;
        }
    }
    return count;
}

/* Publish the marked panels to the map: a tile whose panel mark is set gets
 * MAP_TILE_FLAG_ABILITY_RANGE unless it is blocked or a cross-section surface,
 * and every other tile loses the flag. Returns the number of flagged tiles.
 *
 * target_set_ability_panels ends with it for abilities that have the
 * vertical-fixed or a unit-panels flag; the others end with
 * target_set_all_panels_targeted_if_targetable, which tests the panel's
 * range instead of its mark. */
s32 target_clear_selection_state_of_all_panels(void) {
    map_tile_t* tile;
    target_panel_t* panel;
    s32 count;
    s32 i;

    count = 0;
    for (i = 0; i < 0x200; i++) {
        tile = &g_map_tile_data[i];
        panel = &g_target_panel_data[i];
        if (panel->mark != 0 && !(tile->flags.blocked) && (tile->surface.type) != MAP_SURFACE_CROSS_SECTION) {
            count++;
            tile->ceiling_depth_and_marks.ability_range = 1;
        } else {
            tile->ceiling_depth_and_marks.ability_range = 0;
        }
    }
    return count;
}

/*
 * Marks the targeting panel under `unit` for each simulation slot.
 * The retail loop never advances the unit pointer, so it re-marks the same
 * panel; the shape is kept as compiled.
 */
void target_set_state_for_all_unit_panels(const unit_stats_t* unit, s32 flags) {
    target_panel_t* panel;
    s32 i;
    s32 mark_allies;
    s32 mark_enemies;
    s32 team_diff;
    s16 team_diff_copy;
    s32 team;

    mark_allies = flags & ABILITY_SECONDARY_FLAG_1_ALLY_UNIT_PANELS;
    mark_enemies = flags & ABILITY_SECONDARY_FLAG_1_ENEMY_UNIT_PANELS;
    team = unit->team_flags;
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        if (unit->entd_slot != BATTLE_ENTD_SLOT_NONE) {
            team_diff = team ^ unit->initial_team_flags;
            team_diff_copy = team_diff;
            panel = &g_target_panel_data[map_calculate_location(unit)];
            if (mark_allies != 0 && (team_diff & BATTLE_TEAM_MASK) == 0) {
                panel->mark = 1;
            }
            if (mark_enemies != 0 && (team_diff_copy & BATTLE_TEAM_MASK) != 0) {
                panel->mark = 1;
            }
        }
    }
}

/* Mark the horizontal and vertical targeting runs on both map layers.
 *
 * The target reads the first layer's availability byte unsigned and marks
 * both layers when that byte is nonzero. Sharing the loop index and panel
 * temporaries across the sweeps preserves the target register allocation. */
void target_apply_vertical_fixed(s32 x, s32 y) {
    s32 shared;
    s32 vertical;
    s32 tile_index;
    target_panel_t* panel;
    s32 i;

    x &= 0xFF;
    y &= 0xFF;
    {
        vertical = y;
        for (i = -0x1F; i < 0x20; i++) {
            s32 mx;
            target_panel_t* base = g_target_panel_data;
            target_panel_t* lvl1 = base + 0x100;
            shared = x + i;
            mx = g_map_max_x;
            tile_index = (vertical * mx) + shared;
            panel = &base[tile_index];
            if ((shared >= 0) && (shared < mx) && (panel->remaining_range != 0)) {
                panel->mark = 1;
                lvl1[tile_index].mark = 1;
            }
        }
    }
    {
        shared = x;
        for (i = -0x1F; i < 0x20; i++) {
            target_panel_t* base = g_target_panel_data;
            target_panel_t* lvl1 = base + 0x100;
            vertical = y + i;
            tile_index = (vertical * g_map_max_x) + shared;
            panel = &base[tile_index];
            if ((vertical >= 0) && (vertical < g_map_max_y) && (panel->remaining_range != 0)) {
                panel->mark = 1;
                lvl1[tile_index].mark = 1;
            }
        }
    }
}

/*
 * The .ld places .rodata at 0x80174068 so the switch's jump table lands on the
 * target's table.
 */

/* Build the targeting panels for an action and report how its target is chosen.
 *
 * The skillset's menu type picks the range. Abilities (the default and monster
 * menus, the Elemental ability of the unit's tile, the Draw Out of the chosen
 * katana) use their range data through target_set_ability_panels. Item
 * reaches 1 tile, or 4 with Throw Item or for a Mime. Throw reaches the unit's
 * Move and excludes its own tile. Attack and Charge use the weapon's range, Jump
 * the unit's Jump abilities.
 *
 * Returns a target_panels_e value: INVALID for an absent unit, a katana or
 * ability id out of range or a menu type without an action; NO_TARGETING for the
 * remaining menu types and for an auto ability with range 0 that does not use the
 * weapon's range; NONE when no tile is marked; TILE_ONLY when the action cannot
 * follow a unit (always for Item, Throw, Attack, Charge and Jump); otherwise
 * UNIT_OR_TILE. */
s32 target_set_panels_for_action(const action_command_t* source) {
    action_command_t action;
    unit_stats_t* unit;
    ability_secondary_data_t* ability;
    s32 mode;
    s32 range;
    s32 result;
    u8 flags;
    u8 item;
    u16 ability_id;

    main_util_copy_action_data(source, &action);
    mode = 0;
    if (action.actor_id >= BATTLE_UNIT_SLOT_COUNT) {
        return TARGET_PANELS_INVALID;
    }
    unit = &g_battle_unit_stats[action.actor_id];
    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return TARGET_PANELS_INVALID;
    }
    switch (g_main_action_menu_types_by_skillset[action.skillset_id]) {
    case ACTION_MENU_TYPE_DEFAULT:
    case ACTION_MENU_TYPE_MONSTER:
        ability_id = action.ability_id;
        mode = 1;
        break;
    case ACTION_MENU_TYPE_ITEM_INVENTORY:
        range = ((unit->ability_flags.support[2] & BATTLE_SUPPORT_SET_3_THROW_ITEM) || unit->job_id == JOB_ID_MIME) ? 4
                                                                                                                    : 1;
        mode = 2;
        break;
    case ACTION_MENU_TYPE_WEAPON_INVENTORY:
        mode = 2;
        range = unit->move | 0x80;
        break;
    case ACTION_MENU_TYPE_ELEMENTS:
        ability_id = g_main_terrain_geomancy_abilities[g_map_tile_data[map_calculate_location(unit)].surface.type];
        mode = 1;
        break;
    case ACTION_MENU_TYPE_KATANA_INVENTORY:
        mode = 1;
        item = action.item_or_equipment;
        if (item < ITEM_ID_ASURA_KNIFE || item > ITEM_ID_CHIRIJIRADEN) {
            return TARGET_PANELS_INVALID;
        }
        ability_id = (u8)action.item_or_equipment + (ABILITY_ID_DRAW_OUT_ASURA - ITEM_ID_ASURA_KNIFE);
        break;
    case ACTION_MENU_TYPE_ATTACK:
    case ACTION_MENU_TYPE_CHARGE:
        result = target_set_weapon_attack_panels(&action);
        flags = ABILITY_SECONDARY_FLAG_2_NO_FOLLOW_TARGET;
        break;
    case ACTION_MENU_TYPE_JUMP:
        result = target_set_jump_ability_panels(&action);
        flags = ABILITY_SECONDARY_FLAG_2_NO_FOLLOW_TARGET;
        break;
    case ACTION_MENU_TYPE_BLANK_05:
    case ACTION_MENU_TYPE_UNKNOWN_0F:
        return TARGET_PANELS_INVALID;
    default:
        return TARGET_PANELS_NO_TARGETING;
    }
    if (mode == 1) {
        if (ability_id >= ABILITY_ID_ITEM_FIRST) {
            return TARGET_PANELS_INVALID;
        }
        ability = &g_main_ability_range_data[ability_id];
        range = ability->range;
        flags = ability->flags_2;
        if ((ability->flags_1 & ABILITY_SECONDARY_FLAG_1_AUTO) && range == 0
            && !(ability->flags_1 & ABILITY_SECONDARY_FLAG_1_WEAPON_RANGE)) {
            return TARGET_PANELS_NO_TARGETING;
        }
        action.ability_id = ability_id;
        result = target_set_ability_panels(&action);
    }
    if (mode == 2) {
        result = target_calculate_ability_range_with_map_parameters(unit, range & 0xff);
        flags = ABILITY_SECONDARY_FLAG_2_NO_FOLLOW_TARGET;
    }
    if (result != 0) {
        /* The named temporary is load-bearing; the direct test compiles differently. */
        s32 flag_result = flags & ABILITY_SECONDARY_FLAG_2_NO_FOLLOW_TARGET;

        return flag_result != 0;
    }
    return TARGET_PANELS_NONE;
}
