#include "fft/battle.h"
#include "psx/types.h"

/*
 * Resolves the terrain records for a moving unit's current tile and the tile
 * one step away, as encoded by movement_value: bits 6-7 select the axis and
 * sign, bit 5 selects the map layer and bits 0-1 add extra distance.
 */
void battle_move_get_current_and_destination_tiles(
    battle_unit_misc_data_t* unit, map_tile_t** out_current, map_tile_t** out_target) {
    u8 movement;
    u32 flags;
    s32 dir;
    s32 x;
    s32 y;
    s32 layer;

    x = unit->map_x;
    y = unit->map_y;
    layer = unit->map_z;
    movement = unit->movement_value;
    *out_current = battle_map_get_tile_data_pointer(x, y, layer);
    flags = movement;
    dir = flags >> 6;
    switch (dir) {
    case 0:
        layer = flags >> 5;
        layer &= 1;
        x = unit->map_x;
        x += 1;
        y = unit->map_y;
        x += flags & 3;
        break;
    case 1:
        layer = flags >> 5;
        layer &= 1;
        x = unit->map_x;
        x -= 1;
        y = unit->map_y;
        x -= flags & 3;
        break;
    case 2:
        layer = flags >> 5;
        layer &= 1;
        y = unit->map_y;
        y -= 1;
        x = unit->map_x;
        y -= flags & 3;
        break;
    case 3:
        layer = flags >> 5;
        layer &= 1;
        y = unit->map_y;
        y += 1;
        x = unit->map_x;
        y += flags & 3;
        break;
    default:
        return;
    }
    *out_target = battle_map_get_tile_data_pointer(x, y, layer);
}
