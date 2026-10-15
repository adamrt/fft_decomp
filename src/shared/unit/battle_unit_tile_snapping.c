#include "fft/battle_gfx.h"
#include "fft/battle_unit.h"

/* Snaps a unit to its map tile: the movement destination becomes the tile and the
 * integer world position its center (28 units per tile) at the tile's height. */
void battle_unit_set_move_and_world_coords(battle_unit_t* unit) {
    unit->movement.destination_x = unit->map_x;
    unit->movement.destination_y = unit->map_y;
    unit->movement.destination_z = unit->map_z;
    unit->world_coords.vx = unit->map_x * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
    unit->world_coords.vz = unit->map_y * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
    unit->world_coords.vy = battle_gfx_calculate_world_height_from_world_coords(unit);
    battle_unit_set_real_coords_from_world_coords(unit);
}

/* Moves a rider killed on its mount (pending_attack_result set) off it: the pair
 * is unlinked, the unit takes the relocation tile in dismount as its map position
 * and screen position, its battle stats follow, and the pending result is
 * cleared. */
void battle_unit_set_map_coords_after_death_dismount(battle_unit_t* unit) {
    /* pending_attack_result is -1 when the action killed a rider on its mount. */
    if (unit->pending_attack_result != 0) {
        battle_unit_dismount_rider_and_update_display(unit);
        unit->map_x = unit->dismount.x;
        unit->map_y = unit->dismount.y;
        unit->map_z = unit->dismount.layer;
        battle_unit_set_move_and_world_coords(unit);
        battle_unit_set_tile_position(
            unit->stats->battle_id, unit->map_x, unit->map_y, unit->map_z, (unit->facing / BATTLE_FACING_QUARTER_TURN));
        unit->pending_attack_result = 0;
    }
}
