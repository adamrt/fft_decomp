#include "fft/battle_action.h"
#include "fft/battle_formula.h"
#include "fft/battle_map.h"
#include "fft/battle_move.h"
#include "fft/battle_status.h"
#include "fft/battle_target.h"
#include "psx/types.h"

#define TILE_EDGE_HEIGHT(tile, shift)                                                                                  \
    (tile->height * 2 + tile->depth_half_height.half_height * ((tile->slope_type >> shift) & 3))

/* Checks whether the current target can be knocked back onto tile (x, y).
 *
 * direction is 0 south, 1 west, 2 north, 3 east and selects the facing edges
 * of the target's tile and the destination tile. A destination layer higher
 * than the target's edge is unusable; when only one layer is low enough, the
 * other layer's ceiling must leave at least four half-units of headroom. Of
 * two usable layers the higher one wins. On success the fall height goes to
 * g_action_current_ability.knockback_fall_height and the ability target ID to
 * g_action_current_ability.post_action_target_id, and the destination
 * layer is returned; -1 means off-map, blocked or occupied. */
s32 move_check_knockback_destination(s32 direction, s32 x, s32 y) {
    map_tile_t* tile;
    map_tile_t* lower;
    map_tile_t* upper;
    s32 source_shift;
    s32 destination_shift;
    s32 height;
    s32 lower_height;
    s32 upper_height;
    s32 level;
    s32 fall;
    s32 index;
    s32 upper_index;

    if (x < 0 || y < 0 || x >= g_map_max_x || y >= g_map_max_y) {
        return -1;
    }
    switch (direction) {
    case 3:
        source_shift = 0;
        destination_shift = 2;
        break;
    case 2:
        source_shift = 6;
        destination_shift = 4;
        break;
    case 1:
        source_shift = 2;
        destination_shift = 0;
        break;
    case 0:
        source_shift = 4;
        destination_shift = 6;
        break;
    }
    tile = &g_map_tile_data[map_calculate_location(g_action_target)];
    height = TILE_EDGE_HEIGHT(tile, source_shift);
    index = y * g_map_max_x + x;
    upper_index = index + MAP_TILES_PER_LAYER;
    lower = &g_map_tile_data[index];
    upper = &g_map_tile_data[upper_index];
    if (lower->flags.blocked) {
        lower_height = 0xff;
    } else {
        lower_height = TILE_EDGE_HEIGHT(lower, destination_shift);
    }
    upper_height = 0xff;
    if (!(upper->flags.blocked)) {
        upper_height = TILE_EDGE_HEIGHT(upper, destination_shift);
    }
    if (height < lower_height && height < upper_height) {
        return -1;
    }
    if (height < upper_height && height >= lower_height) {
        upper_height -= upper->ceiling_depth_and_marks.ceiling_depth;
        if (upper_height < height + 4) {
            return -1;
        }
        level = 0;
        fall = height - lower_height;
    } else if (height < lower_height && height >= upper_height) {
        lower_height -= lower->ceiling_depth_and_marks.ceiling_depth;
        if (lower_height < height + 4) {
            return -1;
        }
        level = 1;
        fall = height - upper_height;
    } else if (lower_height >= upper_height) {
        level = 0;
        fall = height - lower_height;
    } else {
        level = 1;
        fall = height - upper_height;
    }
    if (fall < 0) {
        return -1;
    }
    if (target_find_unit_at_tile(x, y, level) != -1) {
        return -1;
    }
    g_action_current_ability.knockback_fall_height = fall;
    g_action_current_ability.post_action_target_id = g_action_current_ability.target_id;
    return level;
}
