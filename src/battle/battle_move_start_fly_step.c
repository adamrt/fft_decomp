#include "fft/battle.h"
#include "psx/types.h"

/* Start the unit's next movement step, dismounting any rider first.
 *
 * Variant of battle_move_start_unit_step that sets the facing through
 * battle_unit_store_animation_facing with animation 0x12: the step byte's top
 * two bits pick the direction, facing and edge offset, and its low two bits
 * extend the destination tile coordinate. A carried rider (mount state MOUNT)
 * takes animation 0x32 with the mount's facing. The caller also passes the
 * current and destination tiles, which this variant never reads. */
void battle_move_start_fly_step(battle_unit_misc_data_t* unit, s32 current_tile, s32 destination_tile) {
    s32 facing;
    u8* step;
    battle_unit_misc_data_t* rider;

    battle_unit_dismount_rider(unit);
    step = &unit->movement_value;
    switch (unit->movement_value >> 6) {
    case 0:
        facing = 0xC00;
        unit->centre_tile_offset = 0x24;
        unit->movement.bytes.destination_x = (u8)(unit->map_x + 1) + (unit->movement_value & 3);
        break;
    case 1:
        facing = 0x400;
        unit->centre_tile_offset = 0x28;
        unit->movement.bytes.destination_x = (u8)(unit->map_x - 1) - (unit->movement_value & 3);
        break;
    case 2:
        facing = 0;
        unit->centre_tile_offset = 0x22;
        unit->movement.bytes.destination_y = (u8)(unit->map_y - 1) - (unit->movement_value & 3);
        break;
    case 3:
        facing = 0x800;
        unit->centre_tile_offset = 0x26;
        unit->movement.bytes.destination_y = (u8)(unit->map_y + 1) + (unit->movement_value & 3);
        break;
    }

    battle_unit_store_animation_facing(0x12, facing, unit);
    if (unit->mount_state == BATTLE_MISC_MOUNT_STATE_MOUNT) {
        rider = battle_unit_get_misc_data_by_misc_id(unit->mount_partner_misc_id);
        if (rider != 0) {
            battle_unit_store_animation_facing(0x32, (s16)unit->facing, rider);
        }
    }
    battle_move_set_velocity_for_contiguous_clamped_steps(unit, &unit->movement_path_count, step);
    battle_move_interpolate_partial(unit, unit->walk_speed.word);
    unit->movement.bytes.destination_z = (*step >> 5) & 1;
}
