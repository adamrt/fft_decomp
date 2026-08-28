#include "fft/battle.h"
#include "psx/types.h"

/* Seat the rider on the mount: link both records, copy the mount's facing,
 * position and map coordinates to the rider and raise the rider above it. */
void battle_unit_mount_rider_onto_unit(battle_unit_misc_data_t* rider, battle_unit_misc_data_t* mount) {
    rider->mount_state = BATTLE_MISC_MOUNT_STATE_RIDER;
    rider->mount_partner_misc_id = mount->unit_id;
    mount->mount_state = BATTLE_MISC_MOUNT_STATE_MOUNT;
    mount->mount_partner_misc_id = rider->unit_id;
    rider->facing = mount->facing;
    rider->camera_facing_quadrant.u = mount->camera_facing_quadrant.u;
    rider->camera_facing_sixteenth.u = mount->camera_facing_sixteenth.u;
    rider->screen = mount->screen;
    rider->screen.vy -= 10;
    rider->real = mount->real;
    rider->current_unit_id_plus_one = 0;
    rider->real.vy += -0xa000;
    rider->map_x = mount->map_x;
    rider->map_y = mount->map_y;
    rider->map_z = mount->map_z;
    rider->status_flags_1_4 |= BATTLE_MISC_STATUS_MOUNTED;
    battle_unit_set_animation_based_on_status(rider);
    rider->animation_script_pos = mount->animation_script_pos;
    battle_unit_update_display_by_misc_id(rider->unit_id);
    battle_unit_update_display_by_misc_id(mount->unit_id);
}
