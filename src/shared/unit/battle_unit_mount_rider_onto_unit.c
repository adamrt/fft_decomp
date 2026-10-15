#include "fft/battle_unit.h"

/* Seat the rider on the mount: link both records, copy the mount's facing,
 * position and map coordinates to the rider and raise the rider above it. */
void battle_unit_mount_rider_onto_unit(battle_unit_t* rider, battle_unit_t* mount) {
    rider->mount_state = BATTLE_UNIT_MOUNT_STATE_RIDER;
    rider->mount_partner_runtime_id = mount->runtime_id;
    mount->mount_state = BATTLE_UNIT_MOUNT_STATE_MOUNT;
    mount->mount_partner_runtime_id = rider->runtime_id;
    rider->facing = mount->facing;
    rider->camera_facing_quadrant = mount->camera_facing_quadrant;
    rider->camera_facing_sixteenth = mount->camera_facing_sixteenth;
    rider->world_coords = mount->world_coords;
    rider->world_coords.vy -= 10;
    rider->real = mount->real;
    rider->standing_on_battle_id_plus_one = 0;
    rider->real.vy += -0xa000;
    rider->map_x = mount->map_x;
    rider->map_y = mount->map_y;
    rider->map_z = mount->map_z;
    rider->status_flags_1_4 |= BATTLE_UNIT_STATUS_MOUNTED;
    battle_unit_set_animation_based_on_status(rider);
    rider->animation.script_pos = mount->animation.script_pos;
    battle_unit_update_display_by_runtime_id(rider->runtime_id);
    battle_unit_update_display_by_runtime_id(mount->runtime_id);
}
