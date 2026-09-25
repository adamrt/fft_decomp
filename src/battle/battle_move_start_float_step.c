#include "fft/battle.h"
#include "psx/types.h"

/* Start the unit's next movement step from the current tile centre.
 *
 * The step byte's top two bits pick the direction, which sets the facing and
 * edge offset; its low two bits extend the destination tile coordinate. The
 * second tile argument is unused. */
void battle_move_start_float_step(battle_unit_misc_data_t* unit, const map_tile_t* from, const map_tile_t* to) {
    u8* step;
    s32 step_count;
    s16 facing;

    battle_unit_dismount_rider_and_update_display(unit);
    step_count = unit->current_edge_height;
    step = &unit->movement_value;
    switch (unit->movement_value >> 6) {
    case 0:
        facing = BATTLE_FACING_EAST;
        unit->centre_tile_offset = 0x2F;
        unit->movement.bytes.destination_x = (u8)(unit->map_x + 1) + (unit->movement_value & 3);
        break;
    case 1:
        facing = BATTLE_FACING_WEST;
        unit->centre_tile_offset = 0x37;
        unit->movement.bytes.destination_x = (u8)(unit->map_x - 1) - (unit->movement_value & 3);
        break;
    case 2:
        facing = BATTLE_FACING_SOUTH;
        unit->centre_tile_offset = 0x2B;
        unit->movement.bytes.destination_y = (u8)(unit->map_y - 1) - (unit->movement_value & 3);
        break;
    case 3:
        facing = BATTLE_FACING_NORTH;
        unit->centre_tile_offset = 0x33;
        unit->movement.bytes.destination_y = (u8)(unit->map_y + 1) + (unit->movement_value & 3);
        break;
    }
    unit->facing = facing;
    battle_unit_set_idle_animation_for_movement(unit);
    battle_move_set_float_step_delta_center_to_edge(unit, step, from, step_count);
    battle_move_interpolate_partial(unit, unit->walk_speed.word);
    unit->movement.bytes.destination_z = (*step >> 5) & 1;
}
