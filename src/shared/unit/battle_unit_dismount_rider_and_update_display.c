#include "fft/battle_unit.h"

/* battle_unit_dismount_rider, followed by a display update of the rider and of
 * the mount. */
void battle_unit_dismount_rider_and_update_display(battle_unit_t* rider) {
    battle_unit_t* mount;

    if (rider->mount_state == BATTLE_UNIT_MOUNT_STATE_RIDER) {
        mount = battle_unit_get_by_runtime_id(rider->mount_partner_runtime_id);
        rider->standing_on_battle_id_plus_one = 0;
        rider->mount_state = BATTLE_UNIT_MOUNT_STATE_NONE;
        rider->mount_partner_runtime_id = 0;
        rider->status_flags_1_4 &= ~BATTLE_UNIT_STATUS_MOUNTED;
        battle_unit_update_display_by_runtime_id(rider->runtime_id);
        if (mount != 0) {
            mount->mount_state = BATTLE_UNIT_MOUNT_STATE_NONE;
            mount->mount_partner_runtime_id = 0;
            battle_unit_update_display_by_runtime_id(mount->runtime_id);
        }
    }
}
