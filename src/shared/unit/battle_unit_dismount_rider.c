#include "fft/battle_unit.h"

/* Unlinks a rider from its mount: both records return to the unmounted state and
 * the rider loses its mounted display flag. Does nothing for a unit that is not a
 * rider. */
void battle_unit_dismount_rider(battle_unit_t* unit) {
    battle_unit_t* mount;
    if (unit->mount_state == BATTLE_UNIT_MOUNT_STATE_RIDER) {
        mount = battle_unit_get_by_runtime_id(unit->mount_partner_runtime_id);
        unit->standing_on_battle_id_plus_one = 0;
        unit->mount_state = BATTLE_UNIT_MOUNT_STATE_NONE;
        unit->mount_partner_runtime_id = 0;
        unit->status_flags_1_4 = unit->status_flags_1_4 & ~BATTLE_UNIT_STATUS_MOUNTED;
        if (mount != 0) {
            mount->mount_state = BATTLE_UNIT_MOUNT_STATE_NONE;
            mount->mount_partner_runtime_id = 0;
        }
    }
}
