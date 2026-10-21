/* Load terrain and Move-Find records and access the packed fields of battle tiles.
 * The original unit also contains the unused helper that restores a unit's move and action. */

#include "fft/battle_map.h"
#include "fft/move.h"
#include "fft/system.h"
#include "fft/unit.h"
#include "psx/libc.h"
#include "psx/types.h"

static s32 tile_selector_15_stub(void);

/* Give a unit back its move and its action by clearing both taken flags. No retail
 * code calls it. */
void turn_move_act_activate(unit_stats_t* unit) {
    unit->movement_taken = 0;
    unit->action_taken = 0;
}

/* Load a terrain chunk: the map's x and y size from its first two bytes, then both 256-tile layers
 * into g_battle_map_tile_data. */
void battle_map_tile_data_copy(u8* data) {
    g_battle_map_terrain_width = *data++;
    g_battle_map_terrain_depth = *data++;
    memcpy(g_battle_map_tile_data, data, sizeof(g_battle_map_tile_data));
}

/* Copy a map's four move-find item tiles into g_move_find_current_map_data. */
void move_find_load_item_data(move_find_map_data_t* map_data) {
    g_move_find_current_map_data = *map_data;
}

/* The tile at (x, y) of a layer, or 0 outside the map or for a layer other than 0
 * and 1. */
battle_map_tile_t* battle_map_tile_get_pointer(s32 x, s32 y, u32 layer) {
    s32 max_x = g_battle_map_terrain_width;
    s32 max_y = g_battle_map_terrain_depth;

    if (x < 0 || x >= max_x || y < 0 || y >= max_y || layer >= 2U) {
        return 0;
    }
    return &g_battle_map_tile_data[(layer << 8) + y * max_x + x];
}

/* Return one packed field of the tile at (x, y, layer), or 0xff for an
 * unknown selector. The tile address is not bounds-checked. */
s32 battle_map_tile_get_value(battle_map_tile_data_selector_e field, s32 x, s32 y, s32 layer) {
    battle_map_tile_t* tile = &g_battle_map_tile_data[(layer << 8) + y * g_battle_map_terrain_width + x];

    switch (field) {
    case BATTLE_MAP_TILE_DATA_SURFACE_TYPE:
        return tile->surface.type;
    case BATTLE_MAP_TILE_DATA_UNKNOWN_01:
        return tile->_unknown_01;
    case BATTLE_MAP_TILE_DATA_HEIGHT:
        return tile->height;
    case BATTLE_MAP_TILE_DATA_HALF_HEIGHT:
        return tile->depth_half_height.half_height;
    case BATTLE_MAP_TILE_DATA_DEPTH:
        return tile->depth_half_height.depth;
    case BATTLE_MAP_TILE_DATA_SLOPE_TYPE:
        return tile->slope_type;
    case BATTLE_MAP_TILE_DATA_CEILING_DEPTH:
        return tile->ceiling_depth_and_marks.ceiling_depth;
    case BATTLE_MAP_TILE_DATA_BLOCKED:
        return tile->flags.blocked;
    case BATTLE_MAP_TILE_DATA_UNTARGETABLE:
        return tile->flags.untargetable;
    case BATTLE_MAP_TILE_DATA_SHADING:
        return tile->flags.shading;
    case BATTLE_MAP_TILE_DATA_UNKNOWN_FLAGS_4_6:
        return tile->flags._unknown_00;
    case BATTLE_MAP_TILE_DATA_FLAT_CAMERA_BLOCK_MASK:
        return tile->camera_block_masks & BATTLE_MAP_TILE_CAMERA_BLOCK_DIRECTION_MASK;
    case BATTLE_MAP_TILE_DATA_MOVE_DESTINATION_MARK:
        return tile->ceiling_depth_and_marks.move_destination;
    case BATTLE_MAP_TILE_DATA_ABILITY_RANGE_MARK:
        return tile->ceiling_depth_and_marks.ability_range;
    }
    return 0xFF;
}

/* Store one packed field of the tile at (x, y, layer): the setter of
 * battle_map_tile_get_value. Returns 0, or -2 for a selector with no case,
 * which includes the two mark selectors. The tile address is not bounds-checked.
 * The event script uses only the flat camera block selector.
 *
 * Selector 15 touches no tile: it calls tile_selector_15_stub until that
 * returns 0, at most 1000 times, and raises battle load exception 0x80 when it
 * never does. The callee always returns 0, so neither the retry nor the exception
 * can run. */
s32 battle_map_tile_set_value(battle_map_tile_data_selector_e field, s32 x, s32 y, s32 layer, s32 value) {
    battle_map_tile_t* tile;
    s32 attempts = 0;
    s32 result = 0;

    tile = &g_battle_map_tile_data[(layer << 8) + y * g_battle_map_terrain_width + x];
    switch (field) {
    case BATTLE_MAP_TILE_DATA_SURFACE_TYPE:
        tile->surface.type = value;
        break;
    case BATTLE_MAP_TILE_DATA_UNKNOWN_01:
        tile->_unknown_01 = value;
        break;
    case BATTLE_MAP_TILE_DATA_HEIGHT:
        tile->height = value;
        break;
    case BATTLE_MAP_TILE_DATA_HALF_HEIGHT:
        tile->depth_half_height.half_height = value;
        break;
    case BATTLE_MAP_TILE_DATA_DEPTH:
        /* A byte store keeps the mask before the shift and the shared switch tail. */
        value = (value & BATTLE_MAP_TILE_DEPTH_VALUE_MASK) << BATTLE_MAP_TILE_DEPTH_SHIFT;
        (*(u8*)&tile->depth_half_height)
            = ((*(u8*)&tile->depth_half_height) & BATTLE_MAP_TILE_HALF_HEIGHT_MASK) | value;
        break;
    case BATTLE_MAP_TILE_DATA_SLOPE_TYPE:
        tile->slope_type = value;
        break;
    case BATTLE_MAP_TILE_DATA_CEILING_DEPTH:
        tile->ceiling_depth_and_marks.ceiling_depth = value;
        break;
    case BATTLE_MAP_TILE_DATA_BLOCKED:
        tile->flags.blocked = value;
        break;
    case BATTLE_MAP_TILE_DATA_UNTARGETABLE:
        tile->flags.untargetable = value;
        break;
    case BATTLE_MAP_TILE_DATA_SHADING:
        tile->flags.shading = value;
        break;
    case BATTLE_MAP_TILE_DATA_UNKNOWN_FLAGS_4_6:
        tile->flags._unknown_00 = value;
        break;
    case BATTLE_MAP_TILE_DATA_FLAT_CAMERA_BLOCK_MASK:
        tile->camera_block_masks = (tile->camera_block_masks & BATTLE_MAP_TILE_CAMERA_BLOCK_STEEP_MASK)
            | (value & BATTLE_MAP_TILE_CAMERA_BLOCK_DIRECTION_MASK);
        break;
    case 15:
        while (attempts < 1000) {
            result = tile_selector_15_stub();
            if (result == 0) {
                break;
            }
            attempts++;
        }
        if (attempts == 1000) {
            system_battle_load_exception_handle(SYSTEM_BATTLE_LOAD_EXCEPTION_MAP_RETRY);
        }
        break;
    default:
        result = -2;
        break;
    }
    return result;
}

/* Selector 15 of battle_map_tile_set_value retries this until it returns 0, which it does at
 * once. The 8-byte frame is one the retail build kept for an unreferenced local. */
static s32 tile_selector_15_stub(void) {
    char unused[8];

    return 0;
}
