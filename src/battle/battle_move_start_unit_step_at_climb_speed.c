#include "fft/battle.h"
#include "fft/battle_move.h"
#include "psx/types.h"

/* Start the unit's next movement step with animation 0x23.
 *
 * Variant of battle_move_start_unit_step that sets the facing through
 * battle_unit_store_animation_facing: the step byte's top two bits pick the
 * direction, facing and edge offset, and its low two bits extend the
 * destination tile coordinate. A carried rider (mount state MOUNT) takes
 * animation 0x32 with the mount's facing. The interpolation scale is
 * step_speed, the 0x2000 climb scale, rather than the walk speed. */
void battle_move_start_unit_step_at_climb_speed(battle_unit_misc_data_t* unit, const map_tile_t* from) {
    u8* step;
    s32 step_count;
    battle_unit_misc_data_t* rider;

    step_count = unit->current_edge_height;
    step = &unit->movement_value;
    switch (unit->movement_value >> 6) {
    case 0:
        unit->centre_tile_offset = 0xB;
        battle_unit_store_animation_facing(0x23, 0xC00, unit);
        unit->movement.bytes.destination_x = (u8)(unit->map_x + 1) + (unit->movement_value & 3);
        break;
    case 1:
        unit->centre_tile_offset = 0xF;
        battle_unit_store_animation_facing(0x23, 0x400, unit);
        unit->movement.bytes.destination_x = (u8)(unit->map_x - 1) - (unit->movement_value & 3);
        break;
    case 2:
        unit->centre_tile_offset = 9;
        battle_unit_store_animation_facing(0x23, 0, unit);
        unit->movement.bytes.destination_y = (u8)(unit->map_y - 1) - (unit->movement_value & 3);
        break;
    case 3:
        unit->centre_tile_offset = 0xD;
        battle_unit_store_animation_facing(0x23, 0x800, unit);
        unit->movement.bytes.destination_y = (u8)(unit->map_y + 1) + (unit->movement_value & 3);
        break;
    }
    if (unit->mount_state == BATTLE_MISC_MOUNT_STATE_MOUNT) {
        rider = battle_unit_get_misc_data_by_misc_id(unit->mount_partner_misc_id);
        if (rider != 0) {
            battle_unit_store_animation_facing(0x32, (s16)unit->facing, rider);
        }
    }
    battle_move_set_unit_step_delta_center_to_edge(unit, step, from, step_count);
    battle_move_interpolate_partial(unit, unit->step_speed);
    unit->movement.bytes.destination_z = (*step >> 5) & 1;
}
