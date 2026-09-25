#include "fft/battle.h"
#include "psx/types.h"

/* Start the unit's next movement step from the current tile centre.
 *
 * Variant of battle_move_start_unit_step_and_dismount_rider without the rider dismount: the step byte's top
 * two bits pick the direction, which sets the facing and edge offset; its low
 * two bits extend the destination tile coordinate. An out-of-range direction
 * keeps the caller's facing, and game state 0x2d leaves the facing unchanged. */
void battle_move_start_unit_step(battle_unit_misc_data_t* unit, const map_tile_t* from, s16 facing) {
    u8* step;
    s32 step_count;

    step_count = unit->current_edge_height;
    step = &unit->movement_value;
    switch (unit->movement_value >> 6) {
    case 0:
        facing = BATTLE_FACING_EAST;
        unit->centre_tile_offset = 3;
        unit->movement.bytes.destination_x = (u8)(unit->map_x + 1) + (unit->movement_value & 3);
        break;
    case 1:
        facing = BATTLE_FACING_WEST;
        unit->centre_tile_offset = 7;
        unit->movement.bytes.destination_x = (u8)(unit->map_x - 1) - (unit->movement_value & 3);
        break;
    case 2:
        facing = BATTLE_FACING_SOUTH;
        unit->centre_tile_offset = 1;
        unit->movement.bytes.destination_y = (u8)(unit->map_y - 1) - (unit->movement_value & 3);
        break;
    case 3:
        facing = BATTLE_FACING_NORTH;
        unit->centre_tile_offset = 5;
        unit->movement.bytes.destination_y = (u8)(unit->map_y + 1) + (unit->movement_value & 3);
        break;
    }
    if (g_battle_game_state != BATTLE_GAME_STATE_ACTION_EXECUTE) {
        unit->facing = facing;
        battle_unit_set_idle_animation_for_movement(unit);
    }
    battle_move_set_unit_step_delta_center_to_edge(unit, step, from, step_count);
    battle_move_interpolate_partial(unit, unit->walk_speed.word);
    unit->movement.bytes.destination_z = (*step >> 5) & 1;
}
