/* Tile pointer and packed field access. */

#include "fft/battle.h"
#include "fft/battle_map.h"
#include "psx/types.h"

void map_copy_xy_coords_and_tile_data(u8* data) {
    g_map_max_x = *data++;
    g_map_max_y = *data++;
    memcpy(g_map_tile_data, data, 0x1000);
}

void map_load_move_find_item_data(map_move_find_item_data_t* map_data) {
    g_battle_current_map_move_find_item_data = *map_data;
}

/* The tile at (x, y) of a layer, or 0 outside the map or for a layer other than 0
 * and 1. */
map_tile_t* map_get_tile_data_pointer(s32 x, s32 y, u32 layer) {
    s32 max_x = g_map_max_x;
    s32 max_y = g_map_max_y;
    if (x < 0 || x >= max_x || y < 0 || y >= max_y || layer >= 2U) {
        return 0;
    }
    return &g_map_tile_data[(layer << 8) + y * max_x + x];
}

/* Returns one packed field of the tile at (x, y, layer), or 0xff for an
 * unknown selector. The tile address is not bounds-checked. */
s32 map_get_tile_data_value(map_tile_data_selector_e field, s32 x, s32 y, s32 layer) {
    map_tile_t* tile;

    tile = &g_map_tile_data[(layer << 8) + y * g_map_max_x + x];
    switch (field) {
    case MAP_TILE_DATA_SURFACE_TYPE:
        return tile->surface.type;
    case MAP_TILE_DATA_UNKNOWN_01:
        return tile->_unknown_01;
    case MAP_TILE_DATA_HEIGHT:
        return tile->height;
    case MAP_TILE_DATA_HALF_HEIGHT:
        return tile->depth_half_height.half_height;
    case MAP_TILE_DATA_DEPTH:
        return tile->depth_half_height.depth;
    case MAP_TILE_DATA_SLOPE_TYPE:
        return tile->slope_type;
    case MAP_TILE_DATA_CEILING_DEPTH:
        return tile->ceiling_depth_and_marks.ceiling_depth;
    case MAP_TILE_DATA_BLOCKED:
        return tile->flags.blocked;
    case MAP_TILE_DATA_UNTARGETABLE:
        return tile->flags.untargetable;
    case MAP_TILE_DATA_SHADING:
        return tile->flags.shading;
    case MAP_TILE_DATA_UNKNOWN_FLAGS_4_6:
        return tile->flags._unknown_00;
    case MAP_TILE_DATA_FLAT_CAMERA_BLOCK_MASK:
        return tile->camera_block_masks & MAP_TILE_CAMERA_BLOCK_DIRECTION_MASK;
    case MAP_TILE_DATA_MOVE_DESTINATION_MARK:
        return tile->ceiling_depth_and_marks.move_destination;
    case MAP_TILE_DATA_ABILITY_RANGE_MARK:
        return tile->ceiling_depth_and_marks.ability_range;
    }
    return 0xFF;
}

/* Store one packed field of the tile at (x, y, layer): the setter of
 * map_get_tile_data_value. Returns 0, or -2 for a selector with no case,
 * which includes the two mark selectors. The tile address is not bounds-checked.
 * The event script uses only the flat camera block selector.
 *
 * Selector 15 touches no tile: it calls map_tile_data_15_stub until that
 * returns 0, at most 1000 times, and raises battle load exception 0x80 when it
 * never does. The callee always returns 0, so neither the retry nor the exception
 * can run. */
s32 map_set_tile_data_value(map_tile_data_selector_e field, s32 x, s32 y, s32 layer, s32 value) {
    map_tile_t* tile;
    s32 attempts;
    s32 result;

    attempts = 0;
    result = 0;
    tile = &g_map_tile_data[(layer << 8) + y * g_map_max_x + x];
    switch (field) {
    case MAP_TILE_DATA_SURFACE_TYPE:
        tile->surface.type = value;
        break;
    case MAP_TILE_DATA_UNKNOWN_01:
        tile->_unknown_01 = value;
        break;
    case MAP_TILE_DATA_HEIGHT:
        tile->height = value;
        break;
    case MAP_TILE_DATA_HALF_HEIGHT:
        tile->depth_half_height.half_height = value;
        break;
    case MAP_TILE_DATA_DEPTH:
        /* A byte store keeps the mask before the shift and the shared switch tail. */
        value = (value & MAP_TILE_DEPTH_VALUE_MASK) << MAP_TILE_DEPTH_SHIFT;
        (*(u8*)&tile->depth_half_height) = ((*(u8*)&tile->depth_half_height) & MAP_TILE_HALF_HEIGHT_MASK) | value;
        break;
    case MAP_TILE_DATA_SLOPE_TYPE:
        tile->slope_type = value;
        break;
    case MAP_TILE_DATA_CEILING_DEPTH:
        tile->ceiling_depth_and_marks.ceiling_depth = value;
        break;
    case MAP_TILE_DATA_BLOCKED:
        tile->flags.blocked = value;
        break;
    case MAP_TILE_DATA_UNTARGETABLE:
        tile->flags.untargetable = value;
        break;
    case MAP_TILE_DATA_SHADING:
        tile->flags.shading = value;
        break;
    case MAP_TILE_DATA_UNKNOWN_FLAGS_4_6:
        tile->flags._unknown_00 = value;
        break;
    case MAP_TILE_DATA_FLAT_CAMERA_BLOCK_MASK:
        tile->camera_block_masks = (tile->camera_block_masks & MAP_TILE_CAMERA_BLOCK_STEEP_MASK)
            | (value & MAP_TILE_CAMERA_BLOCK_DIRECTION_MASK);
        break;
    case 15:
        while (attempts < 1000) {
            result = map_tile_data_15_stub();
            if (result == 0) {
                break;
            }
            attempts++;
        }
        if (attempts == 1000) {
            main_system_handle_battle_load_exception(0x80);
        }
        break;
    default:
        result = -2;
        break;
    }
    return result;
}

/* Selector 15 of map_set_tile_data_value retries this until it returns 0, which it does at
 * once. The 8-byte frame is one the retail build kept for an unreferenced local. */
s32 map_tile_data_15_stub(void) {
    char unused[8];

    return 0;
}
