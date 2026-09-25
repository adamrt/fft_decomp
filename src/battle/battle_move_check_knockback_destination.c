#include "fft/battle.h"
#include "psx/types.h"

/* Half-unit height of a tile edge: base height plus the slope height scaled by
 * that side's two-bit slope factor. */
#define TILE_EDGE_HEIGHT(tile, shift)                                                                                  \
    ((tile)->height * 2                                                                                                \
        + ((tile)->depth_half_height & MAP_TILE_HALF_HEIGHT_MASK) * (((tile)->slope_type >> (shift)) & 3))

/* Checks whether the current target can be knocked back onto tile (x, y).
 *
 * direction is 0 south, 1 west, 2 north, 3 east and selects the facing edges
 * of the target's tile and the destination tile. A destination layer higher
 * than the target's edge is unusable; when only one layer is low enough, the
 * other layer's ceiling must leave at least four half-units of headroom. Of
 * two usable layers the higher one wins. On success the fall height goes to
 * g_current_ability.knockback_fall_height and the ability target ID to
 * g_current_ability.post_action_target_id, and the destination
 * layer is returned; -1 means off-map, blocked or occupied. */
s32 battle_move_check_knockback_destination(s32 direction, s32 x, s32 y) {
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
    tile = &g_battle_map_tile_data[battle_map_calculate_location(g_battle_action_target)];
    height = TILE_EDGE_HEIGHT(tile, source_shift);
    index = y * g_map_max_x + x;
    upper_index = index + 0x100;
    lower = &g_battle_map_tile_data[index];
    upper = &g_battle_map_tile_data[upper_index];
    if (lower->flags_06.value & MAP_TILE_FLAG_BLOCKED) {
        lower_height = 0xff;
    } else {
        lower_height = TILE_EDGE_HEIGHT(lower, destination_shift);
    }
    upper_height = 0xff;
    if (!(upper->flags_06.value & MAP_TILE_FLAG_BLOCKED)) {
        upper_height = TILE_EDGE_HEIGHT(upper, destination_shift);
    }
    if (height < lower_height && height < upper_height) {
        return -1;
    }
    if (height < upper_height && height >= lower_height) {
        upper_height -= upper->ceiling_depth_and_marks & MAP_TILE_CEILING_DEPTH_MASK;
        if (upper_height < height + 4) {
            return -1;
        }
        level = 0;
        fall = height - lower_height;
    } else if (height < lower_height && height >= upper_height) {
        lower_height -= lower->ceiling_depth_and_marks & MAP_TILE_CEILING_DEPTH_MASK;
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
    if (battle_target_get_unit_id_if_tile_targetable(x, y, level) != -1) {
        return -1;
    }
    g_current_ability.knockback_fall_height = fall;
    g_current_ability.post_action_target_id = g_current_ability.target_id;
    return level;
}
