#include "fft/battle.h"
#include "psx/types.h"

/* Validates a lunging-weapon target one or two tiles away in a straight line.
 *
 * Called by battle_target_validate_weapon_target for WEAPON_FLAG_LUNGING. Adjacent targets
 * pass the plain height-overlap test. At distance 2 the middle tile's two
 * layers (surface height, or an occupant's total height) must leave part of
 * the attacker/target height window open; otherwise a unit on the middle tile
 * whose span contains the attacker's mid-height is returned instead.
 * `elevation` is unused. Returns the unit hit, or -1.
 *
 * `total` doubles as the height swap temporary, and the separate upper_index
 * keeps the target's instruction order for the upper-layer tile address. */
s32 battle_target_validate_lunging_target(s32 unit_id, s32 x, s32 y, s32 elevation, s32 target_id) {
    battle_unit_height_data_t height;
    battle_stats_t* unit;
    map_tile_t* lower;
    map_tile_t* upper;
    s32 unit_x;
    s32 unit_y;
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

    unit = &g_battle_unit_stats[unit_id];
    unit_x = unit->x;
    unit_y = unit->position.bits.y;
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

    battle_calculate_unit_height_data(&height, target_id);
    if (height.unit_flags & BATTLE_UNIT_HEIGHT_UNAVAILABLE) {
        target_id = -1;
    }
    target_total = height.total_height;
    target_walking = height.walking_height;
    battle_calculate_unit_height_data(&height, unit_id);
    if (height.unit_flags & BATTLE_UNIT_HEIGHT_UNAVAILABLE) {
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
        low = (unit_walking - 6 < target_walking) ? target_walking : unit_walking - 6;
        limit = unit_total + 2;
        if (target_total < limit) {
            limit = target_total;
        }
        high = limit;
        index = mid_y * g_battle_map_max_x + mid_x;
        lower = &g_battle_map_tile_data[index];
        upper_index = index + 0x100;
        upper = &g_battle_map_tile_data[upper_index];
        lower_height = lower->height * 2 + (lower->depth_half_height & 0x1f);
        upper_height = upper->height * 2 + (upper->depth_half_height & 0x1f);
        lower_id = battle_target_get_unit_id_if_tile_targetable(mid_x, mid_y, 0);
        upper_id = battle_target_get_unit_id_if_tile_targetable(mid_x, mid_y, 1);
        if (lower->flags_06.bits.blocked) {
            lower_height = 0xff;
        }
        if (upper->flags_06.bits.blocked) {
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
            battle_calculate_unit_height_data(&height, lower_id);
            if (!(height.unit_flags & BATTLE_UNIT_HEIGHT_UNAVAILABLE)) {
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
            battle_calculate_unit_height_data(&height, upper_id);
            if (!(height.unit_flags & BATTLE_UNIT_HEIGHT_UNAVAILABLE)) {
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

    middle = (u32)(unit_walking + unit_total) >> 1;
    for (layer = 0; layer < 2; layer++) {
        target_id = battle_target_get_unit_id_if_tile_targetable(mid_x, mid_y, layer);
        battle_calculate_unit_height_data(&height, target_id);
        if (!(height.unit_flags & BATTLE_UNIT_HEIGHT_UNAVAILABLE)) {
            target_walking = height.walking_height;
            target_total = height.total_height;
            if (target_walking < middle && middle < target_total) {
                return target_id;
            }
        }
    }
    return -1;
}
