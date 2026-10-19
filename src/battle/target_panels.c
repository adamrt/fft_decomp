/* Ability and weapon target ranges, affected panels and target eligibility. */

#include "fft/ability.h"
#include "fft/action.h"
#include "fft/battle_map.h"
#include "fft/command.h"
#include "fft/item.h"
#include "fft/job.h"
#include "fft/memory.h"
#include "fft/projectile.h"
#include "fft/skillset.h"
#include "fft/status.h"
#include "fft/target.h"
#include "fft/unit.h"
#include "fft/unit_identity.h"
#include "fft/unit_runtime.h"
#include "psx/types.h"

extern u8 g_ability_calculator_primes[CALCULATOR_PRIME_COUNT];

s32 g_target_sort_nearest_first = 0;

static void target_area_apply_unit_team_eligibility(
    s32 raw_unit_id, u8 allow_allies, u8 allow_enemies, u8 aoe_whole_map);
static void target_range_mark_cardinal_lines(s32 x, s32 y);
static void target_area_build_directional_attack_panels(const action_command_t* source, u8 mode);
static void target_range_mark_tile_in_height_band(s32 x, s32 y, s32 lo, s32 hi);
static void target_area_calculate_linear_attack_tiles(s32 dir, s32 x, s32 y);
static void target_range_calculate_strike_lunge(const unit_stats_t* unit, u8 flags);
static void target_range_clear_untargetable_tiles(void);
static s32 target_range_set_flags_from_marks(void);
static s32 target_area_keep_only_target_tile(const action_command_t* action);
static s32 target_area_run_calculator(const action_command_t* source);
static s32 target_range_set_flags(void);
static s32 target_area_set_for_ability(const action_command_t* action);
static void target_range_mark_unit_panels(const unit_stats_t* unit, s32 flags);
static s32 target_spread_panel_to_neighbors(s32 y, s32 x);
static s32 target_validate_height_overlap(
    s32 attacker_id, s32 target_x, s32 target_y, s32 target_elevation, s32 target_id);
static s32 target_validate_lunge(s32 unit_id, s32 x, s32 y, s32 elevation, s32 target_id);

/* Moldball Virus cannot reach water: for that ability only, clear the range of every panel, on
 * both levels, whose tile has depth. */
static void target_area_check_moldball_virus_depth(s16 ability_id) {
    battle_map_tile_t* tile;
    target_panel_t* panel;
    s32 i;

    if (ability_id != ABILITY_ID_MONSTER_GREAT_MORBOL_MOLDBALL_VIRUS)
        return;
    i = 0;
    panel = g_target_panels;
    tile = g_battle_map_tile_data;
    do {
        if (tile->depth_half_height.depth) {
            panel->remaining_range = 0;
        }
        tile++;
        i += 1;
        panel++;
    } while (i < BATTLE_MAP_TILE_COUNT);
}

/* Clear the range and the mark of each panel whose tile is flagged untargetable.
 * Only the 256 lower-level panels are visited. */
static void target_range_clear_untargetable_panels(void) {
    s32 i = 0;
    battle_map_tile_t* src;
    target_panel_t* dst = g_target_panels;

    src = g_battle_map_tile_data;
    for (; i < BATTLE_MAP_TILES_PER_LAYER; src++, i++, dst++) {
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
static void target_apply_vertical_tolerance(u8 ref_height, u8 tolerance, s32 single_layer) {
    s32 max_delta = tolerance * 2;
    s32 ref = ref_height;
    s32 i;
    s32 j;
    s32 lower;
    s32 upper;
    s32 lower_delta;
    s32 upper_delta;
    battle_map_tile_t* tile;
    target_panel_t* lower_panel;
    target_panel_t* upper_panel;

    for (i = 0; i < BATTLE_MAP_TILES_PER_LAYER; i++) {
        tile = &g_battle_map_tile_data[i];
        lower_panel = &g_target_panels[i];
        if (!tile->flags.blocked) {
            battle_map_tile_depth_t depth = tile->depth_half_height;
            lower = tile->height * 2 + depth.half_height + depth.depth * 2;
        } else {
            lower = -0xFF;
            lower_panel->remaining_range = 0;
            lower_panel->mark = 0;
        }
        j = i + BATTLE_MAP_TILES_PER_LAYER;
        tile = &g_battle_map_tile_data[j];
        upper_panel = &g_target_panels[j];
        if (!tile->flags.blocked) {
            battle_map_tile_depth_t depth = tile->depth_half_height;
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

    for (i = BATTLE_MAP_TILE_COUNT - 1, panel = &g_target_panels[BATTLE_MAP_TILE_COUNT - 1]; i >= 0; i--, panel--) {
        panel->mark = 0;
    }
}

/* Remove the range from every panel, on both levels, within `range` tiles
 * (Manhattan distance) of (x, y). */
static void target_range_remove_close(s32 x, s32 y, s32 range) {
    s32 i;
    s32 j;
    s32 dy;
    s32 dx;
    s32 k;

    for (i = 0; i < g_battle_map_terrain_depth; i++) {
        dy = (y < i) ? (i - y) : (y - i);
        for (j = 0; j < g_battle_map_terrain_width; j++) {
            dx = (x < j) ? (j - x) : (x - j);
            if (range >= (dy + dx)) {
                g_target_panels[(i * g_battle_map_terrain_width) + j].remaining_range = 0;
                k = j + BATTLE_MAP_TILES_PER_LAYER;
                g_target_panels[(i * g_battle_map_terrain_width) + k].remaining_range = 0;
            }
        }
    }
}

/* Repeat the targeting-panel spread pass up to the given count.
 *
 * Each pass visits every panel whose mark byte flags it as a new frontier,
 * clears the mark and spreads from it; passes stop once none spread. A zero
 * count only clears every panel's mark. Every caller passes 0 as the unused
 * second argument. */
static void target_spread_panels(u8 passes, s32 unused) {
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
        for (y = 0; y < g_battle_map_terrain_depth; y++) {
            for (x = 0; x < g_battle_map_terrain_width; x++) {
                panel = &g_target_panels[y * g_battle_map_terrain_width + x];
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
static s32 target_spread_panel_to_neighbors(s32 y, s32 x) {
    target_panel_t* panel;
    s32 changed = 0;
    s32 i;
    s32 dx;
    s32 dy;
    s32 next_x;
    s32 next_y;
    s32 index;
    s32 upper_x;
    s32 range = g_target_panels[y * g_battle_map_terrain_width + x].remaining_range;
    s32 spread_range = range - 1;

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
        if ((next_x >= 0) && (next_y >= 0) && (next_x < g_battle_map_terrain_width)
            && (next_y < g_battle_map_terrain_depth)) {
            index = next_y * g_battle_map_terrain_width + next_x;
            panel = &g_target_panels[index];
            if (panel->remaining_range < range) {
                panel->remaining_range = spread_range;
                upper_x = next_x + BATTLE_MAP_TILES_PER_LAYER;
                g_target_panels[next_y * g_battle_map_terrain_width + upper_x].remaining_range = spread_range;
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
 * Float and UNIT_MOVEMENT_SET_3_MOVE_IN_WATER stand on the water surface;
 * UNIT_MOVEMENT_SET_3_WALK_ON_WATER stands one level below it. A rider stands
 * 2 half steps higher, and its mount's Float and movement abilities decide the rest. */
static u8 unit_get_effective_height(const unit_stats_t* unit) {
    battle_map_tile_t* tile;
    battle_map_tile_depth_t terrain_depth;
    s32 height;
    u8 effective_height;
    u8 water_depth;
    u8 ignores_water = 0;
    u8 flags;

    tile = &g_battle_map_tile_data[battle_map_tile_calculate_unit_index(unit)];
    terrain_depth = tile->depth_half_height;
    flags = unit->mount_info;
    height = tile->height * 2 + (terrain_depth.half_height);
    water_depth = tile->depth_half_height.depth * 2;
    effective_height = height;
    if (flags & UNIT_MOUNT_INFO_FLAG_RIDER) {
        unit = &g_unit_stats[flags & UNIT_MOUNT_INFO_PARTNER_ID_MASK];
        height += 2;
        effective_height = height;
    }
    flags = unit->ability_flags.movement[2];
    if ((flags & UNIT_MOVEMENT_SET_3_FLOAT)
        || (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FLOAT)] & STATUS_BYTE_MASK(STATUS_ID_FLOAT))) {
        effective_height += 2;
        ignores_water = 1;
    }
    if (water_depth != 0) {
        if (ignores_water || (flags & UNIT_MOVEMENT_SET_3_MOVE_IN_WATER)) {
            effective_height += water_depth;
        } else if (flags & UNIT_MOVEMENT_SET_3_WALK_ON_WATER) {
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
static void target_range_calculate_arc(const unit_stats_t* unit, u8 range, u8 flags) {
    u8 height = unit_get_effective_height(unit);
    u8 tile_height;
    s32 ux;
    s32 uy;
    s32 x;
    s32 y;
    s32 dy;
    s32 dist;
    s32 level;
    s32 index;
    battle_map_tile_t* tile;

    ux = unit->x;
    uy = unit->position.y;
    for (y = 0; y < g_battle_map_terrain_depth; y++) {
        if (uy < y) {
            dy = y - uy;
        } else {
            dy = uy - y;
        }
        for (x = 0; x < g_battle_map_terrain_width; x++) {
            if (ux < x) {
                dist = x - ux + dy;
            } else {
                dist = ux - x + dy;
            }
            if (dist >= 3) {
                for (level = 0; level < 2; level++) {
                    index = (level << 8) + y * g_battle_map_terrain_width + x;
                    tile = &g_battle_map_tile_data[index];
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
static void target_range_calculate_weapon(const unit_stats_t* unit) {
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
    index = x + y * g_battle_map_terrain_width;
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
    data = &g_item_weapon_data[weapon];
    range = data->range;
    flags = data->flags;
    panel = &g_target_panels[index];
    upper = &g_target_panels[index + BATTLE_MAP_TILES_PER_LAYER];
    panel->remaining_range = range + 1;
    upper->remaining_range = range + 1;
    panel->mark = 1;
    if (flags & (WEAPON_FLAG_STRIKING | WEAPON_FLAG_LUNGING)) {
        target_range_calculate_strike_lunge(unit, flags);
    } else if (flags & WEAPON_FLAG_DIRECT) {
        target_spread_panels(range, 0);
        target_range_remove_close(x, y, 2);
    } else {
        target_range_calculate_arc(unit, range, flags);
    }
    target_range_clear_untargetable_tiles();
}

/*
 * Mark the striking or lunging weapon range. A striking weapon (WEAPON_FLAG_STRIKING)
 * covers the four adjacent tiles from 6 half steps below to 5 above the unit; a
 * lunging weapon covers the tiles 1 and 2 away in a straight line, from 8 and then
 * 6 half steps below to 7 above.
 */
static void target_range_calculate_strike_lunge(const unit_stats_t* unit, u8 flags) {
    u8 height = unit_get_effective_height(unit);
    s32 x;
    s32 y;
    s32 i;
    s32 lo;
    s32 hi;

    x = unit->x;
    y = unit->position.y;
    if (flags & WEAPON_FLAG_STRIKING) {
        lo = height - 6;
        hi = height + 5;
        target_range_mark_tile_in_height_band(x - 1, y, lo, hi);
        target_range_mark_tile_in_height_band(x + 1, y, lo, hi);
        target_range_mark_tile_in_height_band(x, y - 1, lo, hi);
        target_range_mark_tile_in_height_band(x, y + 1, lo, hi);
    } else {
        for (i = 1; i < 3; i++) {
            lo = height + (i - 5) * 2;
            hi = height + 7;
            target_range_mark_tile_in_height_band(x - i, y, lo, hi);
            target_range_mark_tile_in_height_band(x + i, y, lo, hi);
            target_range_mark_tile_in_height_band(x, y - i, lo, hi);
            target_range_mark_tile_in_height_band(x, y + i, lo, hi);
        }
    }
}

/* Give range 1 to the panels of map position (x, y), on either level, whose
 * height lies in lo..hi. Heights are in half steps (height * 2 + slope half
 * height + depth * 2); a negative lo counts as 0 and a position off the map is
 * ignored. The strike and lunge weapon ranges are built from it. */
static void target_range_mark_tile_in_height_band(s32 x, s32 y, s32 lo, s32 hi) {
    s32 level;
    s32 idx;
    s32 height;
    battle_map_tile_t* tile;
    target_panel_t* panel;
    battle_map_tile_depth_t flags;

    if ((x >= 0) && (x < g_battle_map_terrain_width) && (y >= 0) && (y < g_battle_map_terrain_depth)) {
        level = 0;
        if (lo < 0) {
            lo = 0;
        }
        do {
            idx = (level << 8) + (y * g_battle_map_terrain_width) + x;
            tile = &g_battle_map_tile_data[idx];
            panel = &g_target_panels[idx];
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
static void target_range_clear_untargetable_tiles(void) {
    s32 i = 0;
    target_panel_t* dst = g_target_panels;
    battle_map_tile_t* src = g_battle_map_tile_data;
    for (; i < BATTLE_MAP_TILE_COUNT; i++) {
        if (src->flags.untargetable)
            dst->remaining_range = 0;
        src++;
        dst++;
    }
}

/* Mark every tile the acting unit's Jump abilities can reach.
 *
 * Horizontal range and vertical reach start at 1/2 (8 and 16 for a Mime) and
 * grow to the best learned Jump record; panels are spread from the unit's
 * tile, and each map position keeps the higher of its two layers when that
 * layer lies within the vertical window. Returns the number of tiles marked,
 * or 0 when the other layer at the unit's position is taller and passable.
 *
 * The target initializes the lower-layer panel giv first (t1 = panels,
 * t2 = t1 + 0x500). That needs `upper_panels` derived from `panels` and the
 * per-tile panel pointers taken upper layer first, because loop.c keeps its
 * giv list in reverse discovery order. */
static s32 target_range_set_for_jump(const action_command_t* source) {
    s16 abilities[0x14];
    u8 mp_costs[0x18];
    u8 ability_ct[0x48];
    u8 usable[0x18];
    u8 turns[0x18];
    action_command_t action;
    unit_stats_t* unit;
    battle_map_tile_t* tile;
    battle_map_tile_t* upper;
    battle_map_tile_t* lower;
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
    ability_jump_record_t* jump;
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
    action_copy_data(source, &action);
    unit = &g_unit_stats[action.actor_id];
    vertical = 2;
    if (unit->job_id == JOB_ID_MIME) {
        range = 8;
        vertical = 0x10;
    }
    location = battle_map_tile_calculate_unit_index(unit);
    tile = &g_battle_map_tile_data[location];
    tile_height = tile->height;
    height = tile->depth_half_height.half_height + tile_height * 2;
    count = skillset_get_unit_ability_data(
        action.actor_id, action.skillset_id, abilities, mp_costs, ability_ct, 0, usable, turns);
    for (i = 0; i < count; i++) {
        ability_id = abilities[i];
        if (ability_id >= ABILITY_ID_JUMP_FIRST && ability_id < ABILITY_ID_CHARGE_FIRST) {
            jump_index = ability_id - ABILITY_ID_JUMP_FIRST;
            jump = &g_ability_jump_data[(u16)jump_index];
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
    target_clear_panels();
    if (location < BATTLE_MAP_TILES_PER_LAYER) {
        other_location = location + BATTLE_MAP_TILES_PER_LAYER;
    } else {
        other_location = location - BATTLE_MAP_TILES_PER_LAYER;
    }
    panel = &g_target_panels[location];
    other_panel = &g_target_panels[other_location];
    panel->remaining_range = range + 1;
    other_panel->remaining_range = range + 1;
    if (location < BATTLE_MAP_TILES_PER_LAYER) {
        panel->mark = 1;
    } else {
        other_panel->mark = 1;
    }
    target_spread_panels(range, 0);
    tile = &g_battle_map_tile_data[other_location];
    panel->remaining_range = 0;
    other_panel->remaining_range = 0;
    other_height = tile->depth_half_height.half_height + tile->height * 2;
    if ((tile->flags.blocked) == 0 && height < other_height) {
        return 0;
    }
    marked = 0;
    panels = g_target_panels;
    upper_panels = panels + BATTLE_MAP_TILES_PER_LAYER;
    for (i = 0; i < BATTLE_MAP_TILES_PER_LAYER; i++) {
        other_panel = &upper_panels[i];
        panel = &panels[i];
        lower = &g_battle_map_tile_data[i];
        upper = &g_battle_map_tile_data[i + BATTLE_MAP_TILES_PER_LAYER];
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
 * the unit's own tile afterwards. Blocked and untargetable tiles are never
 * marked. Returns the number of marked panels. */
static s32 target_range_spread_from_unit(const unit_stats_t* unit, u8 range) {
    target_panel_t* panel;
    target_panel_t* upper;
    target_panel_t* panels;
    battle_map_tile_t* tile;
    s32 index;
    s32 i;
    s32 count;
    u8 remaining;

    index = unit->position.y * g_battle_map_terrain_width + unit->x;
    target_clear_panels();
    remaining = (range & 0x7f) + 1;
    panel = &g_target_panels[index];
    upper = &g_target_panels[index + BATTLE_MAP_TILES_PER_LAYER];
    panel->remaining_range = remaining;
    upper->remaining_range = remaining;
    panel->mark = 1;
    target_spread_panels(range, 0);
    if (range & 0x80) {
        panel->remaining_range = 0;
        upper->remaining_range = 0;
    }
    count = 0;
    tile = g_battle_map_tile_data;
    for (i = 0, panels = g_target_panels; i < BATTLE_MAP_TILE_COUNT; i++) {
        /* A byte collision test retains the original single-mask branch. */
        if (panels->remaining_range != 0 && !(((*(u8*)&tile[i].flags) & BATTLE_MAP_TILE_COLLISION_MASK))) {
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
static s32 target_range_set_for_ability(const action_command_t* source) {
    action_command_t action;
    unit_stats_t* unit;
    ability_secondary_data_t* ability;
    target_panel_t* panels;
    target_panel_t* origin;
    target_panel_t* seed;
    target_panel_t* seed_upper;
    battle_map_tile_t* tile;
    battle_map_tile_depth_t terrain_depth;
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

    action_copy_data(source, &action);
    unit = &g_unit_stats[action.actor_id];
    ability = &g_ability_range_data[action.ability_id];
    range = ability->range;
    aoe = ability->aoe;
    flags_1 = ability->flags_1;
    vertical = ability->vertical;
    flags_4 = ability->flags_4;
    x = unit->x;
    y = unit->position.y;
    location = battle_map_tile_calculate_unit_index(unit);
    tile = &g_battle_map_tile_data[location];
    terrain_depth = tile->depth_half_height;
    elevation = tile->height * 2 + (terrain_depth.half_height) + tile->depth_half_height.depth * 2;
    panels = g_target_panels;
    origin_offset = location * sizeof(target_panel_t);
    origin = (target_panel_t*)((s32)panels + origin_offset);
    index = y * g_battle_map_terrain_width + x;
    if (range == ABILITY_RANGE_WHOLE_MAP) {
        for (i = 0; i < BATTLE_MAP_TILE_COUNT; i++) {
            panels[i].remaining_range = 1;
            panels[i].mark = 0;
        }
    } else {
        for (i = 0; i < BATTLE_MAP_TILE_COUNT; i++) {
            panels[i].remaining_range = 0;
            panels[i].mark = 0;
        }
        seed = &g_target_panels[index];
        seed_upper = &g_target_panels[index + BATTLE_MAP_TILES_PER_LAYER];
        seed->remaining_range = range + 1;
        seed_upper->remaining_range = range + 1;
        seed->mark = 1;
        if (flags_1 & ABILITY_SECONDARY_FLAG_1_WEAPON_RANGE) {
            target_range_calculate_weapon(unit);
            seed->remaining_range = 0;
            seed_upper->remaining_range = 0;
        } else {
            target_spread_panels(range, 0);
        }
    }
    if (flags_1 & ABILITY_SECONDARY_FLAG_1_CANNOT_TARGET_SELF) {
        origin->remaining_range = 0;
    }
    if (flags_1 & ABILITY_SECONDARY_FLAG_1_CARDINAL_TARGET_RANGE) {
        location = x;
        target_range_mark_cardinal_lines(location, y);
    }
    if (flags_1 & ABILITY_SECONDARY_FLAG_1_VERTICAL_TOLERANCE) {
        target_apply_vertical_tolerance(elevation, vertical, 0);
    }
    if (aoe == 0 || (flags_4 & ABILITY_SECONDARY_FLAG_4_DIRECT_TARGETING)) {
        target_range_clear_untargetable_panels();
    }
    if (flags_1 & (ABILITY_SECONDARY_FLAG_1_ENEMY_UNIT_PANELS | ABILITY_SECONDARY_FLAG_1_ALLY_UNIT_PANELS)) {
        target_range_mark_unit_panels(unit, flags_1);
    }
    if (flags_1
        & (ABILITY_SECONDARY_FLAG_1_CARDINAL_TARGET_RANGE | ABILITY_SECONDARY_FLAG_1_ENEMY_UNIT_PANELS
            | ABILITY_SECONDARY_FLAG_1_ALLY_UNIT_PANELS)) {
        return target_range_set_flags_from_marks();
    }
    return target_range_set_flags();
}

/* Build the targeting panels for a weapon attack: the weapon's range around the
 * acting unit, without the unit's own tile. Returns the number of tiles in range. */
static s32 target_range_set_for_weapon(const action_command_t* source) {
    action_command_t action;
    unit_stats_t* unit;
    s32 idx;

    action_copy_data(source, &action);
    unit = &g_unit_stats[action.actor_id];
    idx = unit->position.y * g_battle_map_terrain_width + unit->x;
    target_clear_panels();
    target_range_calculate_weapon(unit);
    {
        target_panel_t* panel;
        target_panel_t* upper_panel;
        panel = &g_target_panels[idx];
        upper_panel = &g_target_panels[idx + BATTLE_MAP_TILES_PER_LAYER];
        panel->remaining_range = 0;
        upper_panel->remaining_range = 0;
        return target_range_set_flags();
    }
}

/* Publish the panels in range to the map: a tile whose panel has range left gets
 * BATTLE_MAP_TILE_FLAG_ABILITY_RANGE unless it is blocked or a cross-section surface,
 * and every other tile loses the flag. Returns the number of flagged tiles. */
static s32 target_range_set_flags(void) {
    battle_map_tile_t* tile;
    target_panel_t* panel;
    s32 count = 0;
    s32 i;

    for (i = 0; i < BATTLE_MAP_TILE_COUNT; i++) {
        tile = &g_battle_map_tile_data[i];
        panel = &g_target_panels[i];
        if (panel->remaining_range != 0 && !(tile->flags.blocked)
            && (tile->surface.type) != BATTLE_MAP_SURFACE_CROSS_SECTION) {
            count++;
            tile->ceiling_depth_and_marks.ability_range = 1;
        } else {
            tile->ceiling_depth_and_marks.ability_range = 0;
        }
    }
    return count;
}

/* Publish the marked panels to the map: a tile whose panel mark is set gets
 * BATTLE_MAP_TILE_FLAG_ABILITY_RANGE unless it is blocked or a cross-section surface,
 * and every other tile loses the flag. Returns the number of flagged tiles.
 *
 * target_range_set_for_ability ends with it for abilities that have the
 * cardinal-range or a unit-panels flag; the others end with
 * target_range_set_flags, which tests the panel's
 * range instead of its mark. */
static s32 target_range_set_flags_from_marks(void) {
    battle_map_tile_t* tile;
    target_panel_t* panel;
    s32 count = 0;
    s32 i;

    for (i = 0; i < BATTLE_MAP_TILE_COUNT; i++) {
        tile = &g_battle_map_tile_data[i];
        panel = &g_target_panels[i];
        if (panel->mark != 0 && !(tile->flags.blocked) && (tile->surface.type) != BATTLE_MAP_SURFACE_CROSS_SECTION) {
            count++;
            tile->ceiling_depth_and_marks.ability_range = 1;
        } else {
            tile->ceiling_depth_and_marks.ability_range = 0;
        }
    }
    return count;
}

/*
 * Meant to mark the panels of allied or enemy units, but the retail loop runs
 * for each of the 21 unit slots without advancing the unit pointer, so it only
 * marks the caster's own panel: under the ally flag when the caster's team
 * matches its initial team, under the enemy flag when it differs.
 */
static void target_range_mark_unit_panels(const unit_stats_t* unit, s32 flags) {
    target_panel_t* panel;
    s32 i;
    s32 mark_allies = flags & ABILITY_SECONDARY_FLAG_1_ALLY_UNIT_PANELS;
    s32 mark_enemies = flags & ABILITY_SECONDARY_FLAG_1_ENEMY_UNIT_PANELS;
    s32 team_diff;
    s16 team_diff_copy;
    s32 team = unit->team_flags;

    for (i = 0; i < UNIT_SLOT_COUNT; i++) {
        if (unit->active_battle_id != UNIT_ACTIVE_BATTLE_ID_NONE) {
            team_diff = team ^ unit->initial_team_flags;
            team_diff_copy = team_diff;
            panel = &g_target_panels[battle_map_tile_calculate_unit_index(unit)];
            if (mark_allies != 0 && (team_diff & UNIT_TEAM_MASK) == 0) {
                panel->mark = 1;
            }
            if (mark_enemies != 0 && (team_diff_copy & UNIT_TEAM_MASK) != 0) {
                panel->mark = 1;
            }
        }
    }
}

/* For a cardinal-range ability, mark the panels in range along the caster's row
 * and column (up to 31 tiles either way), which target_range_set_for_ability
 * then publishes.
 *
 * Only the lower layer's range is tested, and a hit marks both layers. Sharing
 * the loop index and panel temporaries across the sweeps preserves the target
 * register allocation. */
static void target_range_mark_cardinal_lines(s32 x, s32 y) {
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
            target_panel_t* base = g_target_panels;
            target_panel_t* upper_panels = base + BATTLE_MAP_TILES_PER_LAYER;
            shared = x + i;
            mx = g_battle_map_terrain_width;
            tile_index = (vertical * mx) + shared;
            panel = &base[tile_index];
            if ((shared >= 0) && (shared < mx) && (panel->remaining_range != 0)) {
                panel->mark = 1;
                upper_panels[tile_index].mark = 1;
            }
        }
    }
    {
        shared = x;
        for (i = -0x1F; i < 0x20; i++) {
            target_panel_t* base = g_target_panels;
            target_panel_t* upper_panels = base + BATTLE_MAP_TILES_PER_LAYER;
            vertical = y + i;
            tile_index = (vertical * g_battle_map_terrain_width) + shared;
            panel = &base[tile_index];
            if ((vertical >= 0) && (vertical < g_battle_map_terrain_depth) && (panel->remaining_range != 0)) {
                panel->mark = 1;
                upper_panels[tile_index].mark = 1;
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
 * katana) use their range data through target_range_set_for_ability. Item
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
target_panels_e target_range_set_for_action(const action_command_t* source) {
    action_command_t action;
    unit_stats_t* unit;
    ability_secondary_data_t* ability;
    s32 mode;
    u8 range;
    s32 result;
    u8 flags;
    u8 item;
    u16 ability_id;

    /* mode 1 takes the range from the ability's range data; mode 2 spreads `range`
     * directly, bit 7 leaving out the unit's own tile. */
    action_copy_data(source, &action);
    mode = 0;
    if (action.actor_id >= UNIT_SLOT_COUNT) {
        return TARGET_PANELS_INVALID;
    }
    unit = &g_unit_stats[action.actor_id];
    if (unit->active_battle_id == UNIT_ACTIVE_BATTLE_ID_NONE) {
        return TARGET_PANELS_INVALID;
    }
    switch (g_command_menu_types_by_skillset[action.skillset_id]) {
    case ACTION_MENU_TYPE_DEFAULT:
    case ACTION_MENU_TYPE_MONSTER:
        ability_id = action.ability_id;
        mode = 1;
        break;
    case ACTION_MENU_TYPE_ITEM_INVENTORY:
        range
            = ((unit->ability_flags.support[2] & UNIT_SUPPORT_SET_3_THROW_ITEM) || unit->job_id == JOB_ID_MIME) ? 4 : 1;
        mode = 2;
        break;
    case ACTION_MENU_TYPE_WEAPON_INVENTORY:
        mode = 2;
        range = unit->move | 0x80;
        break;
    case ACTION_MENU_TYPE_ELEMENTS:
        ability_id = g_terrain_geomancy_abilities[g_battle_map_tile_data[battle_map_tile_calculate_unit_index(unit)]
                .surface.type];
        mode = 1;
        break;
    case ACTION_MENU_TYPE_KATANA_INVENTORY:
        mode = 1;
        item = action.item_id;
        if (item < ITEM_ID_ASURA_KNIFE || item > ITEM_ID_CHIRIJIRADEN) {
            return TARGET_PANELS_INVALID;
        }
        ability_id = action.item_id + (ABILITY_ID_DRAW_OUT_ASURA - ITEM_ID_ASURA_KNIFE);
        break;
    case ACTION_MENU_TYPE_ATTACK:
    case ACTION_MENU_TYPE_CHARGE:
        result = target_range_set_for_weapon(&action);
        flags = ABILITY_SECONDARY_FLAG_2_NO_FOLLOW_TARGET;
        break;
    case ACTION_MENU_TYPE_JUMP:
        result = target_range_set_for_jump(&action);
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
        ability = &g_ability_range_data[ability_id];
        range = ability->range;
        flags = ability->flags_2;
        if ((ability->flags_1 & ABILITY_SECONDARY_FLAG_1_AUTO) && range == 0
            && !(ability->flags_1 & ABILITY_SECONDARY_FLAG_1_WEAPON_RANGE)) {
            return TARGET_PANELS_NO_TARGETING;
        }
        action.ability_id = ability_id;
        result = target_range_set_for_ability(&action);
    }
    if (mode == 2) {
        result = target_range_spread_from_unit(unit, range);
        flags = ABILITY_SECONDARY_FLAG_2_NO_FOLLOW_TARGET;
    }
    if (result != 0) {
        /* The named temporary is load-bearing; the direct test compiles differently. */
        s32 flag_result = flags & ABILITY_SECONDARY_FLAG_2_NO_FOLLOW_TARGET;

        return flag_result != 0;
    }
    return TARGET_PANELS_NONE;
}

/* Mark the tiles an action affects once its target is chosen, by skillset menu type: abilities go
 * through the green-panel pass, Arithmeticks through the calculator, and Item, Throw, Attack, Jump
 * and Charge mark only the target tile. Returns 0 when tiles were marked, 1 when none were, 2 for
 * menu types without targeting, and -1 for an invalid actor, target, katana, ability id or target
 * tile. */
s32 target_area_set_for_action(const action_command_t* source) {
    action_command_t action;
    unit_stats_t* unit;
    u16 ability_id;
    u8 item;
    s32 mode;
    s32 result;

    /* mode 1 runs the green-panel pass for an ability, mode 2 keeps only the target
     * tile, and mode 0 means the calculator already did the work. */
    action_copy_data(source, &action);
    mode = 1;
    if (action.actor_id >= UNIT_SLOT_COUNT) {
        return -1;
    }
    unit = &g_unit_stats[action.actor_id];
    if (unit->active_battle_id == UNIT_ACTIVE_BATTLE_ID_NONE) {
        return -1;
    }
    if (action.targeting_type == TARGET_TYPE_UNIT && action.target_id >= UNIT_SLOT_COUNT) {
        return -1;
    }
    switch (g_command_menu_types_by_skillset[action.skillset_id]) {
    case ACTION_MENU_TYPE_DEFAULT:
    case ACTION_MENU_TYPE_MONSTER:
        ability_id = action.ability_id;
        break;
    case ACTION_MENU_TYPE_ITEM_INVENTORY:
    case ACTION_MENU_TYPE_WEAPON_INVENTORY:
    case ACTION_MENU_TYPE_ATTACK:
    case ACTION_MENU_TYPE_JUMP:
    case ACTION_MENU_TYPE_CHARGE:
        mode = 2;
        break;
    case ACTION_MENU_TYPE_ARITHMETICKS:
        result = target_area_run_calculator(&action);
        mode = 0;
        break;
    case ACTION_MENU_TYPE_ELEMENTS:
        ability_id = ability_get_elemental_id(unit);
        break;
    case ACTION_MENU_TYPE_KATANA_INVENTORY:
        item = action.item_id;
        if (item < ITEM_ID_ASURA_KNIFE || item > ITEM_ID_CHIRIJIRADEN) {
            return -1;
        }
        ability_id = action.item_id + (ABILITY_ID_DRAW_OUT_ASURA - ITEM_ID_ASURA_KNIFE);
        break;
    case ACTION_MENU_TYPE_BLANK_05:
    case ACTION_MENU_TYPE_UNKNOWN_0F:
        return -1;
    default:
        return 2;
    }
    if (mode == 1) {
        if (ability_id >= ABILITY_ID_ITEM_FIRST) {
            return -1;
        }
        action.ability_id = ability_id;
        result = target_area_set_for_ability(&action);
    }
    if (mode == 2) {
        result = target_area_keep_only_target_tile(&action);
    }
    if (result == 0) {
        return 1;
    }
    return result == -1 ? -1 : 0;
}

/* Re-validate the acting unit's stored target and mark the target's tile.
 *
 * The weapon's flags select the trajectory test: ARC takes
 * projectile_arc_set_and_validate, DIRECT the direct-weapon check (skipped
 * for an adjacent tile when the skillset menu type is ITEM_INVENTORY), LUNGING
 * target_validate_lunge, and anything else the plain height-overlap test.
 * Attack and Charge skillsets, and abilities flagged ABILITY_SECONDARY_FLAG_1_WEAPON_RANGE,
 * use the equipped weapon; other skillsets use DIRECT. For TARGET_TYPE_UNIT they
 * also copy the target unit's position into the local action copy, which nothing reads
 * afterward: the tests use the actor's stored target position.
 * Returns -1 for an absent actor, a shield in a weapon hand or an invalid target id,
 * otherwise 0, whether or not a unit was marked.
 *
 * One `hand` variable carries both hands, so the right-hand load stays after
 * the left-hand mask; `target` is reused for the result unit, which keeps it a
 * global pseudo and gives the target's v1 for the copied position. */
s32 target_validate_for_weapon(const action_command_t* source) {
    action_command_t action;
    s16 x;
    unit_stats_t* unit;
    unit_stats_t* target;
    s32 flags;
    u8 unit_id;
    u8 menu_type;
    u8 ability_flags;
    s32 hand;
    s32 mask;
    u8 weapon;
    u32 weapon_id;
    battle_map_tile_t* tile;
    s16 y;
    s16 elevation;
    s32 target_id;
    s32 distance_x;
    s32 distance_y;
    s32 distance;
    s32 result;

    action_copy_data(source, &action);
    unit_id = action.actor_id;
    if (unit_id >= UNIT_SLOT_COUNT) {
        return -1;
    }
    unit = &g_unit_stats[unit_id];
    if (unit->active_battle_id == UNIT_ACTIVE_BATTLE_ID_NONE) {
        return -1;
    }
    menu_type = g_command_menu_types_by_skillset[action.skillset_id];
    ability_flags = 0;
    if (menu_type == ACTION_MENU_TYPE_DEFAULT || menu_type == ACTION_MENU_TYPE_MONSTER) {
        /* Retail indexes by the id's low byte, so abilities 0x100 and up read another ability's flags. */
        ability_flags = g_ability_range_data[(u8)action.ability_id].flags_1;
    }
    if (menu_type == ACTION_MENU_TYPE_ATTACK || menu_type == ACTION_MENU_TYPE_CHARGE
        || (ability_flags & ABILITY_SECONDARY_FLAG_1_WEAPON_RANGE)) {
        /* An empty left hand contributes 0; an equipped right hand wins. */
        hand = unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON];
        mask = -(hand != ITEM_ID_NONE);
        weapon = hand & mask;
        hand = unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON];
        if (hand != ITEM_ID_NONE) {
            weapon = hand;
        }
        if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FROG)] & STATUS_BYTE_MASK(STATUS_ID_FROG))
            || (unit->unit_flags & UNIT_FLAG_MONSTER) || weapon == ITEM_ID_NONE) {
            weapon = 0;
        }
        weapon_id = weapon;
        if (weapon_id >= ITEM_ID_SHIELD_FIRST) {
            return -1;
        }
        flags = g_item_weapon_data[weapon_id].flags;
    } else {
        if (action.targeting_type == TARGET_TYPE_UNIT) {
            if (action.target_id >= UNIT_SLOT_COUNT) {
                return -1;
            }
            target = &g_unit_stats[action.target_id];
            action.target_position.vx = target->x;
            action.target_position.vz = target->position.y;
            action.target_position.vy = target->position.higher_elevation;
        }
        flags = WEAPON_FLAG_DIRECT;
    }
    x = unit->action_command.target_position.vx;
    y = unit->action_command.target_position.vz;
    elevation = unit->action_command.target_position.vy;
    target_id = target_find_unit_at_tile(x, y, elevation);
    if (target_id < 0) {
        target_id = -1;
    }
    if (flags & WEAPON_FLAG_ARC) {
        result = projectile_arc_set_and_validate(unit_id, &unit->action_command.target_position, target_id);
    } else if (flags & WEAPON_FLAG_DIRECT) {
        distance_x = x - unit->x;
        if (distance_x < 0) {
            distance_x = -distance_x;
        }
        distance_y = y - unit->position.y;
        if (distance_y < 0) {
            distance_y = -distance_y;
        }
        distance = distance_x + distance_y;
        if (menu_type == ACTION_MENU_TYPE_ITEM_INVENTORY && distance < 2) {
            result = target_id;
        } else {
            result = projectile_direct_check_to_target(unit_id, &unit->action_command.target_position, target_id);
        }
    } else if (flags & WEAPON_FLAG_LUNGING) {
        result = target_validate_lunge(unit_id, x, y, elevation, target_id);
    } else {
        result = target_validate_height_overlap(unit_id, x, y, elevation, target_id);
    }
    target_area_clear_flags();
    if (result >= 0 && result < UNIT_SLOT_COUNT) {
        target = &g_unit_stats[result];
        tile = &g_battle_map_tile_data[battle_map_tile_calculate_unit_index(target)];
        tile->ceiling_depth_and_marks.targeted = 1;
    }
    return 0;
}

/* Validates a lunging-weapon target one or two tiles away in a straight line.
 *
 * Called by target_validate_for_weapon for WEAPON_FLAG_LUNGING. Adjacent targets
 * pass the plain height-overlap test. At distance 2 the middle tile's two
 * layers (surface height, or an occupant's total height) must leave part of
 * the attacker/target height window open; otherwise a unit on the middle tile
 * whose span contains the attacker's mid-height is returned instead.
 * `elevation` is unused. Returns the unit hit, or -1.
 *
 * `total` doubles as the height swap temporary, and the separate upper_index
 * keeps the target's instruction order for the upper-layer tile address. */
static s32 target_validate_lunge(s32 unit_id, s32 x, s32 y, s32 elevation, s32 target_id) {
    unit_height_data_t height;
    unit_stats_t* unit = &g_unit_stats[unit_id];
    battle_map_tile_t* lower;
    battle_map_tile_t* upper;
    s32 unit_x = unit->x;
    s32 unit_y = unit->position.y;
    s32 dx;
    s32 dy;
    s32 distance;
    u8 target_walking;
    u8 target_total;
    u8 unit_walking;
    u8 unit_total;
    s32 mid_x;
    s32 mid_y;
    u8 low;
    u8 high;
    u8 lower_height;
    u8 upper_height;
    s32 lower_id;
    s32 upper_id;
    s32 swap_id;
    u8 total;
    u32 middle;
    s32 layer;
    s32 index;
    s32 upper_index;
    s32 limit;

    /* Only a straight line of one or two tiles qualifies. */
    if (unit_x < x) {
        dx = x - unit_x;
    } else {
        dx = unit_x - x;
    }
    if (unit_y < y) {
        dy = y - unit_y;
    } else {
        dy = unit_y - y;
    }

    if (dx == 0) {
        if (dy == 1) {
            distance = 1;
        } else if (dy == 2) {
            distance = 2;
        } else {
            return -1;
        }
    } else if (dy != 0) {
        return -1;
    } else if (dx == 1) {
        distance = 1;
    } else if (dx == 2) {
        distance = 2;
    } else {
        return -1;
    }

    unit_calculate_height_data(&height, target_id);
    if (height.unit_flags & UNIT_HEIGHT_UNAVAILABLE) {
        target_id = -1;
    }
    target_total = height.total_height;
    target_walking = height.walking_height;

    unit_calculate_height_data(&height, unit_id);
    if (height.unit_flags & UNIT_HEIGHT_UNAVAILABLE) {
        return -1;
    }
    unit_walking = height.walking_height;
    unit_total = height.total_height;

    if (distance == 1) {
        if (target_id == -1) {
            return -1;
        }
        if (target_walking < unit_total + 2 && target_total + 2 >= unit_walking) {
            return target_id;
        }
        return -1;
    }

    mid_x = (unit_x + x) / 2;
    mid_y = (unit_y + y) / 2;
    if (distance == 2 && target_id != -1 && target_walking < unit_total + 2 && target_total >= unit_walking) {
        /* The strike passes through the height window low..high unless the middle tile closes it. */
        low = (unit_walking - 6 < target_walking) ? target_walking : unit_walking - 6;
        limit = unit_total + 2;
        if (target_total < limit) {
            limit = target_total;
        }
        high = limit;

        /* Both layers of the middle tile. */
        index = mid_y * g_battle_map_terrain_width + mid_x;
        lower = &g_battle_map_tile_data[index];
        upper_index = index + BATTLE_MAP_TILES_PER_LAYER;
        upper = &g_battle_map_tile_data[upper_index];
        lower_height = lower->height * 2 + lower->depth_half_height.half_height;
        upper_height = upper->height * 2 + upper->depth_half_height.half_height;
        lower_id = target_find_unit_at_tile(mid_x, mid_y, 0);
        upper_id = target_find_unit_at_tile(mid_x, mid_y, 1);
        /* A blocked layer counts as absent (0xff) and is skipped below. */
        if (lower->flags.blocked) {
            lower_height = 0xff;
        }
        if (upper->flags.blocked) {
            upper_height = 0xff;
        }

        if (lower_height > upper_height) {
            total = upper_height;
            upper_height = lower_height;
            lower_height = total;
            swap_id = upper_id;
            upper_id = lower_id;
            lower_id = swap_id;
        }

        if (lower_height != 0xff) {
            unit_calculate_height_data(&height, lower_id);
            if (!(height.unit_flags & UNIT_HEIGHT_UNAVAILABLE)) {
                lower_height = height.total_height;
            }
            if (low < lower_height) {
                low = lower_height;
            }
            if (high < lower_height) {
                low = 0xff;
            }
        }

        if (upper_height != 0xff) {
            unit_calculate_height_data(&height, upper_id);
            if (!(height.unit_flags & UNIT_HEIGHT_UNAVAILABLE)) {
                total = height.total_height;
                if (low >= upper_height && total >= high) {
                    low = 0xff;
                }
            }
        }

        if (high > low) {
            return target_id;
        }
    }

    /* Blocked or no valid target: hit the unit on the middle tile whose span contains
     * the attacker's mid-height. */
    middle = (u32)(unit_walking + unit_total) >> 1;
    for (layer = 0; layer < 2; layer++) {
        target_id = target_find_unit_at_tile(mid_x, mid_y, layer);
        unit_calculate_height_data(&height, target_id);
        if (!(height.unit_flags & UNIT_HEIGHT_UNAVAILABLE)) {
            target_walking = height.walking_height;
            target_total = height.total_height;
            if (target_walking < middle && middle < target_total) {
                return target_id;
            }
        }
    }
    return -1;
}

/* Return the target only when its vertical span overlaps the attacker's.
 *
 * The coordinate arguments are unused here but are part of the caller ABI. */
static s32 target_validate_height_overlap(
    s32 attacker_id, s32 target_x, s32 target_y, s32 target_elevation, s32 target_id) {
    unit_height_data_t height_data;
    u32 target_walking_height;
    u32 target_total_height;
    u32 attacker_total_height;
    u32 attacker_walking_height;

    unit_calculate_height_data(&height_data, target_id);
    if ((height_data.unit_flags & UNIT_HEIGHT_UNAVAILABLE) != 0) {
        return -1;
    }
    target_walking_height = height_data.walking_height;
    target_total_height = height_data.total_height;
    unit_calculate_height_data(&height_data, attacker_id);
    if ((height_data.unit_flags & UNIT_HEIGHT_UNAVAILABLE) != 0) {
        return -1;
    }
    attacker_total_height = height_data.total_height;
    attacker_walking_height = height_data.walking_height;
    if (target_walking_height < attacker_total_height && target_total_height >= attacker_walking_height) {
        return target_id;
    }
    return -1;
}

/* Clear BATTLE_MAP_TILE_FLAG_TARGETED on all 512 tiles. */
void target_area_clear_flags(void) {
    battle_map_tile_t* tile;
    s32 i;

    for (i = 0; i < BATTLE_MAP_TILE_COUNT; i++) {
        tile = &g_battle_map_tile_data[i];
        tile->ceiling_depth_and_marks.targeted = 0;
    }
}

/* Marks the tiles of every unit selected by a Calculator (Arithmeticks) action.
 *
 * The type ability picks the compared value (CT, capped at 100 and forced to
 * 100 for the caster, level, EXP, or whole-unit height, where half heights never
 * qualify); the multiplier ability tests it for primality or divisibility by
 * 5, 4 or 3. Units that are absent, crystallized, jumping, treasure, or
 * mounts carrying a rider are skipped, and a value of 0 never qualifies.
 * Returns the number of marked units, or -1 when the command does not name
 * one type and one multiplier ability. */
static s32 target_area_run_calculator(const action_command_t* source) {
    unit_height_data_t height;
    action_command_t action;
    unit_stats_t* actor;
    unit_stats_t* unit;
    battle_map_tile_t* tile;
    s16 type;
    s16 multiplier;
    u8* type_entry;
    u8* multiplier_entry;
    u8 type_flags;
    u8 multiplier_flags;
    u8 value;
    s32 count = 0;
    s32 hit;
    s32 i;
    s32 j;

    action_copy_data(source, &action);
    type = action.calculator_type;
    multiplier = action.calculator_multiplier;
    actor = &g_unit_stats[action.actor_id];
    if (type < ABILITY_ID_MATH_FIRST || type >= ABILITY_ID_REACTION_FIRST) {
        return -1;
    }
    if (multiplier < ABILITY_ID_MATH_FIRST || multiplier >= ABILITY_ID_REACTION_FIRST) {
        return -1;
    }
    type_entry = &g_ability_math_rsm_data[type - ABILITY_ID_MATH_FIRST];
    multiplier_entry = &g_ability_math_rsm_data[multiplier - ABILITY_ID_MATH_FIRST];
    type_flags = *type_entry;
    multiplier_flags = *multiplier_entry;
    if (!(type_flags & CALCULATOR_ATTRIBUTE_MASK) || !(multiplier_flags & CALCULATOR_MULTIPLE_MASK)
        || (type_flags & CALCULATOR_MULTIPLE_MASK) || (multiplier_flags & CALCULATOR_ATTRIBUTE_MASK)) {
        return -1;
    }
    target_area_clear_flags();
    for (i = 0; i < UNIT_SLOT_COUNT; i++) {
        unit = &g_unit_stats[i];
        if (unit->active_battle_id == UNIT_ACTIVE_BATTLE_ID_NONE) {
            continue;
        }
        if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)]
            & (STATUS_BYTE_MASK(STATUS_ID_CRYSTAL) | STATUS_BYTE_MASK(STATUS_ID_JUMP))) {
            continue;
        }
        if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TREASURE)] & STATUS_BYTE_MASK(STATUS_ID_TREASURE)) {
            continue;
        }
        if (unit->mount_info & UNIT_MOUNT_INFO_FLAG_MOUNT) {
            continue;
        }
        if (type_flags & CALCULATOR_ATTRIBUTE_CT) {
            value = unit->ct;
            if (value > UNIT_TURN_CT_THRESHOLD || unit == actor) {
                value = UNIT_TURN_CT_THRESHOLD;
            }
        } else if (type_flags & CALCULATOR_ATTRIBUTE_LEVEL) {
            value = unit->level;
        } else if (type_flags & CALCULATOR_ATTRIBUTE_EXP) {
            value = unit->experience;
        } else {
            unit_calculate_height_data(&height, i);
            value = height.walking_height;
            if (value & 1) {
                continue;
            }
            value >>= 1;
        }
        if (value == 0) {
            continue;
        }
        hit = 0;
        if (multiplier_flags & CALCULATOR_MULTIPLE_PRIME) {
            for (j = 0; j < CALCULATOR_PRIME_COUNT; j++) {
                if (g_ability_calculator_primes[j] == value) {
                    hit = 1;
                    break;
                }
            }
        } else if ((multiplier_flags & CALCULATOR_MULTIPLE_FIVE) && value % 5 == 0) {
            hit = 1;
        } else if ((multiplier_flags & CALCULATOR_MULTIPLE_FOUR) && (value & 3) == 0) {
            hit = 1;
        } else if ((multiplier_flags & CALCULATOR_MULTIPLE_THREE) && value % 3 == 0) {
            hit = 1;
        }
        if (hit != 0) {
            tile = &g_battle_map_tile_data[battle_map_tile_calculate_unit_index(unit)];
            count++;
            tile->ceiling_depth_and_marks.targeted = 1;
        }
    }
    return count;
}

/* Leave only the action's target tile marked as targeted. Returns 1, or -1
 * when that tile is off the map or blocked. */
static s32 target_area_keep_only_target_tile(const action_command_t* action) {
    action_command_t action_copy;
    u8 x;
    u8 y;
    u8 level;
    battle_map_tile_t* tile;

    action_copy_data(action, &action_copy);
    /* The byte locals keep only the low byte of each 16-bit coordinate, as retail does. */
    x = action_copy.target_position.vx;
    y = action_copy.target_position.vz;
    level = action_copy.target_position.vy;
    if (x < g_battle_map_terrain_width) {
        if (y < g_battle_map_terrain_depth) {
            if (level < 2) {
                tile = &g_battle_map_tile_data[(level << 8) + y * g_battle_map_terrain_width + x];
                if ((tile->flags.blocked) == 0) {
                    target_area_clear_flags();
                    tile->ceiling_depth_and_marks.targeted = 1;
                    return 1;
                }
            }
        }
    }
    return -1;
}

/*
 * Mark the tiles an ability affects around its chosen target (its own tile for
 * linear and three-direction abilities) and return how many are targeted, or -1
 * when that center is off the map or blocked.
 *
 * `location` carries the column into the index sum: the extra copies give the
 * index pseudo the allocation priority the target needs (a2), while keeping the
 * `(elevation * 256 + y * width) + x` operand order.
 */
static s32 target_area_set_for_ability(const action_command_t* action) {
    action_command_t action_copy;
    ability_secondary_data_t* ability;
    unit_stats_t* unit;
    unit_stats_t* target;
    battle_map_tile_t* tile;
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

    action_copy_data(action, &action_copy);
    ability = &g_ability_range_data[action_copy.ability_id];
    flags_1 = ability->flags_1;
    flags_2 = ability->flags_2;
    aoe = ability->aoe;
    vertical = ability->vertical;
    unit_id = action_copy.actor_id;
    unit = &g_unit_stats[unit_id];

    /* Direct targeting takes the weapon-range path below (no spread) and keeps only
     * these four targeting flags. */
    if (ability->flags_4 & ABILITY_SECONDARY_FLAG_4_DIRECT_TARGETING) {
        flags_1 |= ABILITY_SECONDARY_FLAG_1_WEAPON_RANGE;
        flags_2 &= ABILITY_SECONDARY_FLAG_2_CANNOT_HIT_CASTER | ABILITY_SECONDARY_FLAG_2_RANDOM_FIRE
            | ABILITY_SECONDARY_FLAG_2_NO_FOLLOW_TARGET | ABILITY_SECONDARY_FLAG_2_TOP_DOWN;
    }

    if (flags_2 & (ABILITY_SECONDARY_FLAG_2_THREE_DIRECTIONS | ABILITY_SECONDARY_FLAG_2_LINEAR_ATTACK)) {
        x = unit->x;
        y = unit->position.y;
        elevation = unit->position.higher_elevation;
    } else if (action_copy.targeting_type != TARGET_TYPE_UNIT) {
        x = (u8)action_copy.target_position.vx;
        y = (u8)action_copy.target_position.vz;
        elevation = (u8)action_copy.target_position.vy;
    } else {
        target = &g_unit_stats[action_copy.target_id];
        x = target->x;
        y = target->position.y;
        elevation = target->position.higher_elevation;
    }

    if (x >= g_battle_map_terrain_width) {
        return -1;
    }
    if (y >= g_battle_map_terrain_depth || elevation >= 2) {
        return -1;
    }

    location = x;
    index = location;
    index = elevation * 256 + y * g_battle_map_terrain_width + location;
    tile = &g_battle_map_tile_data[index];
    if (tile->flags.blocked) {
        return -1;
    }

    /* Center height in half steps, including the water depth. Byte access retains the
     * original packed load and field-extraction order. */
    height = tile->height * 2 + ((*(u8*)&tile->depth_half_height) & BATTLE_MAP_TILE_HALF_HEIGHT_MASK)
        + ((*(u8*)&tile->depth_half_height) >> 5) * 2;

    if (aoe == ABILITY_RANGE_WHOLE_MAP) {
        cursor = g_target_panels;
        for (i = 0; i < BATTLE_MAP_TILE_COUNT; i++) {
            cursor[i].remaining_range = 1;
            cursor[i].mark = 0;
        }
    } else {
        cursor = g_target_panels;
        for (i = 0; i < BATTLE_MAP_TILE_COUNT; i++) {
            cursor[i].remaining_range = 0;
            cursor[i].mark = 0;
        }
        panel = &g_target_panels[index];
        panel->remaining_range = aoe + 1;
        /* Seed the spread from the center position's lower-layer panel, which the spread
         * walks (the trailing `index & 0xff` store marks it a second time). */
        if (index < BATTLE_MAP_TILES_PER_LAYER) {
            other_panel = &g_target_panels[index + BATTLE_MAP_TILES_PER_LAYER];
            panel->mark = 1;
        } else {
            other_panel = &g_target_panels[index - BATTLE_MAP_TILES_PER_LAYER];
            other_panel->mark = 1;
        }
        g_target_panels[index & 0xff].mark = 1;
        if (!(flags_1 & ABILITY_SECONDARY_FLAG_1_WEAPON_RANGE)) {
            panel->remaining_range = aoe + 1;
            other_panel->remaining_range = aoe + 1;
            target_spread_panels(aoe, 0);
            target_apply_vertical_tolerance(height, vertical, (flags_2 & ABILITY_SECONDARY_FLAG_2_TOP_DOWN) != 0);
            target_area_check_moldball_virus_depth(action_copy.ability_id);
        }
    }

    cursor = &g_target_panels[battle_map_tile_calculate_unit_index(unit)];
    if (flags_2 & ABILITY_SECONDARY_FLAG_2_CANNOT_HIT_CASTER) {
        cursor->remaining_range = 0;
    }
    if (flags_2 & ABILITY_SECONDARY_FLAG_2_LINEAR_ATTACK) {
        target_area_build_directional_attack_panels(&action_copy, 1);
    }
    if (flags_2 & ABILITY_SECONDARY_FLAG_2_THREE_DIRECTIONS) {
        target_area_build_directional_attack_panels(&action_copy, 3);
    }
    if (flags_2 & (ABILITY_SECONDARY_FLAG_2_CAN_TARGET_ENEMIES | ABILITY_SECONDARY_FLAG_2_CAN_TARGET_ALLIES)) {
        target_area_apply_unit_team_eligibility(unit_id, flags_2 & ABILITY_SECONDARY_FLAG_2_CAN_TARGET_ALLIES,
            flags_2 & ABILITY_SECONDARY_FLAG_2_CAN_TARGET_ENEMIES, aoe == ABILITY_RANGE_WHOLE_MAP);
    }

    /* Without team targeting every tile in range counts; with it only the marked tiles do.
     * A byte collision test retains the original single-mask branch. */
    count = 0;
    if (!(flags_2 & (ABILITY_SECONDARY_FLAG_2_CAN_TARGET_ENEMIES | ABILITY_SECONDARY_FLAG_2_CAN_TARGET_ALLIES))) {
        tile = g_battle_map_tile_data;
        for (i = 0, marked = g_target_panels; i < BATTLE_MAP_TILE_COUNT; i++) {
            if (marked->remaining_range != 0 && !(((*(u8*)&tile[i].flags) & BATTLE_MAP_TILE_COLLISION_MASK))) {
                count++;
                tile[i].ceiling_depth_and_marks.targeted = 1;
            } else {
                tile[i].ceiling_depth_and_marks.targeted = 0;
            }
            marked++;
        }
        return count;
    }
    tile = g_battle_map_tile_data;
    for (i = 0, marked = g_target_panels; i < BATTLE_MAP_TILE_COUNT; i++) {
        if (marked->mark != 0 && !(((*(u8*)&tile[i].flags) & BATTLE_MAP_TILE_COLLISION_MASK))) {
            count++;
            tile[i].ceiling_depth_and_marks.targeted = 1;
        } else {
            tile[i].ceiling_depth_and_marks.targeted = 0;
        }
        marked++;
    }
    return count;
}

/* Filters unit-occupied targeting panels by team eligibility.
 *
 * The low byte of `raw_unit_id` selects the acting unit, which is treated as
 * same-team even if its initial and live team fields differ. `allow_allies`
 * and `allow_enemies` are raw nonzero flag values. Unless the ability AoE covers
 * the whole map, every nonempty panel left after filtering is marked selected. */
static void target_area_apply_unit_team_eligibility(
    s32 raw_unit_id, u8 allow_allies, u8 allow_enemies, u8 aoe_whole_map) {
    s32 relation;
    u8 team_flags;
    s32 panel_index;
    target_panel_t* panel;
    s32 i;
    s32 actor_id;

    raw_unit_id &= 0xff;
    team_flags = g_unit_stats[raw_unit_id].team_flags;
    actor_id = raw_unit_id;
    for (i = 0; i < UNIT_SLOT_COUNT; i++) {
        if (target_is_unit_untargetable_and_store_tile(i, &panel_index) == 0) {
            relation = team_flags ^ g_unit_stats[i].initial_team_flags;
            if (actor_id == i) {
                relation = 0;
            }
            panel = &g_target_panels[panel_index];
            if (panel->remaining_range != 0) {
                if (relation & UNIT_TEAM_MASK) {
                    if (allow_enemies == 0) {
                        panel->remaining_range = 0;
                    } else {
                        panel->mark = 1;
                    }
                } else if (allow_allies != 0) {
                    panel->mark = 1;
                } else {
                    panel->remaining_range = 0;
                }
            }
        }
    }
    if (!aoe_whole_map) {
        for (i = 0; i < BATTLE_MAP_TILE_COUNT; i++) {
            panel = &g_target_panels[i];
            if (panel->remaining_range != 0) {
                panel->mark = 1;
            }
        }
    }
}

/* Store a unit's tile and report whether targeting must reject the unit. */
s32 target_is_unit_untargetable_and_store_tile(s32 unit_id, s32* tile_index) {
    unit_stats_t* unit = &g_unit_stats[unit_id];

    *tile_index = battle_map_tile_calculate_unit_index(unit);
    if (unit->active_battle_id == UNIT_ACTIVE_BATTLE_ID_NONE) {
        return 1;
    } else if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)]
        & (STATUS_BYTE_MASK(STATUS_ID_CRYSTAL) | STATUS_BYTE_MASK(STATUS_ID_JUMP))) {
        return 1;
    } else if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TREASURE)]
        & STATUS_BYTE_MASK(STATUS_ID_TREASURE)) {
        return 1;
    } else if (unit->mount_info & UNIT_MOUNT_INFO_FLAG_MOUNT) {
        return 1;
    }
    return 0;
}

/* Narrow an ability's panels to the straight lines that leave the acting unit
 * toward its target: mode 1 for a linear attack, mode 3 for a three-direction
 * attack.
 *
 * On each axis where the target tile differs from the unit's, the line in that
 * direction is kept; mode 3 also keeps the two perpendicular lines. Every panel
 * off the kept lines loses its range, and the marks that identified the lines are
 * cleared. The call also turns on nearest-first ordering of the target list
 * (g_target_sort_nearest_first). */
static void target_area_build_directional_attack_panels(const action_command_t* source, u8 mode) {
    action_command_t action;
    unit_stats_t* unit;
    target_panel_t* panel;
    s32 target_x;
    s32 target_y;
    s32 x;
    s32 y;
    s32 i;

    action_copy_data(source, &action);
    g_target_sort_nearest_first = 1;
    unit = &g_unit_stats[action.actor_id];
    x = unit->x;
    y = unit->position.y;
    target_x = action.target_position.vx;
    target_y = action.target_position.vz;
    if (x < target_x) {
        target_area_calculate_linear_attack_tiles(TARGET_LINE_EAST, x, y);
        if (mode == 3) {
            target_area_calculate_linear_attack_tiles(TARGET_LINE_NORTH, x, y);
            target_area_calculate_linear_attack_tiles(TARGET_LINE_SOUTH, x, y);
        }
    }
    if (target_x < x) {
        target_area_calculate_linear_attack_tiles(TARGET_LINE_WEST, x, y);
        if (mode == 3) {
            target_area_calculate_linear_attack_tiles(TARGET_LINE_NORTH, x, y);
            target_area_calculate_linear_attack_tiles(TARGET_LINE_SOUTH, x, y);
        }
    }
    if (y < target_y) {
        target_area_calculate_linear_attack_tiles(TARGET_LINE_NORTH, x, y);
        if (mode == 3) {
            target_area_calculate_linear_attack_tiles(TARGET_LINE_EAST, x, y);
            target_area_calculate_linear_attack_tiles(TARGET_LINE_WEST, x, y);
        }
    }
    if (target_y < y) {
        target_area_calculate_linear_attack_tiles(TARGET_LINE_SOUTH, x, y);
        if (mode == 3) {
            target_area_calculate_linear_attack_tiles(TARGET_LINE_WEST, x, y);
            target_area_calculate_linear_attack_tiles(TARGET_LINE_EAST, x, y);
        }
    }
    for (i = 0; i < BATTLE_MAP_TILE_COUNT; i++) {
        panel = &g_target_panels[i];
        if (panel->mark == 0) {
            panel->remaining_range = 0;
        } else {
            panel->mark = 0;
        }
    }
}

/* Mark the panels along a straight line from (x, y) in one compass direction
 * (TARGET_LINE_*), the starting tile included.
 *
 * On both map levels a target_panel_t with remaining_range gets mark set;
 * the table is g_target_panels. The walk stops
 * at the map edge or after 32 tiles, and any other direction value marks nothing.
 * `dir` is reused as the panel index. */
static void target_area_calculate_linear_attack_tiles(s32 dir, s32 x, s32 y) {
    s32 i;
    s32 dx;
    s32 dy;
    s32 map_width;
    target_panel_t* panel;

    switch (dir) {
    case TARGET_LINE_EAST:
        dx = 1;
        dy = 0;
        break;
    case TARGET_LINE_WEST:
        dx = -1;
        dy = 0;
        break;
    case TARGET_LINE_NORTH:
        dx = 0;
        dy = 1;
        break;
    case TARGET_LINE_SOUTH:
        dx = 0;
        dy = -1;
        break;
    default:
        return;
    }

    i = 0;
    do {
        if (x < 0) {
            return;
        }
        map_width = g_battle_map_terrain_width;
        if (x >= map_width) {
            return;
        }
        if (y < 0) {
            return;
        }
        if (y >= g_battle_map_terrain_depth) {
            return;
        }
        dir = y * map_width + x;
        panel = &g_target_panels[dir];
        if (panel->remaining_range != 0) {
            panel->mark = 1;
        }
        dir += BATTLE_MAP_TILES_PER_LAYER;
        panel = &g_target_panels[dir];
        y += dy;
        if (panel->remaining_range != 0) {
            panel->mark = 1;
        }
        i += 1;
        x += dx;
    } while (i < 0x20);
}
