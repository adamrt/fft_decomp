#include "fft/battle.h"
#include "fft/battle_map.h"
#include "psx/types.h"

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
