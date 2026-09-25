#include "fft/battle.h"
#include "fft/map.h"

/* Bitfield view of map_tile_t byte 3. Reading it as bitfields reproduces the
 * target's separate reload of the byte for each test; masking the shared
 * u8 field lets CSE merge them. */
typedef struct battle_unit_placement_tile_bits {
    map_tile_surface_t surface;
    u8 _unknown01;
    u8 field_02;
    u8 slope_half_height : 5;
    u8 depth : 3;
    u8 slope_type;
    u8 field_05;
    map_tile_flags_06_t flags_06;
    u8 camera_block_masks;
} battle_unit_placement_tile_bits_t;

/* Validate a unit's placement tile.
 *
 * Returns -1 when the position is off the map, the tile is unwalkable, too
 * steep or deep for the unit's movement abilities, lava or an obstacle, or
 * already occupied by an earlier unit slot at the same elevation; 0 otherwise. */
s32 battle_unit_validate_placement(s32 unit_idx) {
    battle_stats_t* unit = &g_battle_unit_stats[unit_idx];
    battle_stats_t* other;
    battle_unit_placement_tile_bits_t* tile;
    u8 x;
    u8 y;
    u8 higher_elevation;
    s32 i;

    x = unit->x;
    y = unit->position.bits.y;
    higher_elevation = unit->position.bits.higher_elevation;
    if (x >= g_map_max_x) {
        return -1;
    }
    if (y >= g_map_max_y) {
        return -1;
    }
    tile = (battle_unit_placement_tile_bits_t*)&g_battle_map_tile_data[battle_map_calculate_location(unit)];
    if (tile->flags_06.value & MAP_TILE_COLLISION_MASK) {
        return -1;
    }
    if (tile->slope_half_height >= 3) {
        return -1;
    }
    if ((unit->movement_abilities[1] & BATTLE_MOVEMENT_SET_2_CANNOT_ENTER_WATER) && tile->depth != 0
        && !(unit->movement_abilities[2] & BATTLE_MOVEMENT_SET_3_FLOAT)) {
        return -1;
    }
    if (tile->depth >= 3
        && !(unit->movement_abilities[2]
            & (BATTLE_MOVEMENT_SET_3_WALK_ON_WATER | BATTLE_MOVEMENT_SET_3_MOVE_IN_WATER
                | BATTLE_MOVEMENT_SET_3_MOVE_UNDERWATER | BATTLE_MOVEMENT_SET_3_FLOAT))) {
        return -1;
    }
    if (tile->surface.bits.type == MAP_SURFACE_LAVA || tile->surface.bits.type == MAP_SURFACE_OBSTACLE) {
        return -1;
    }
    for (i = 0; i < unit_idx; i++) {
        other = &g_battle_unit_stats[i];
        if (other->entd_slot != BATTLE_ENTD_SLOT_NONE && other->x == x && other->position.bits.y == y
            && other->position.bits.higher_elevation == higher_elevation) {
            return -1;
        }
    }
    return 0;
}
