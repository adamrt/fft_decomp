#include "fft/battle_unit.h"

/* Seats one unit on another by unit id (battle_unit_mount_rider_onto_unit).
 * Returns 0 when either record is missing. No retail code calls it. */
s32 battle_unit_mount_by_runtime_ids(u32 rider_runtime_id, u32 mount_runtime_id) {
    battle_unit_t* rider;
    battle_unit_t* mount;

    rider = battle_unit_get_by_runtime_id(rider_runtime_id & 0xffff);
    mount = battle_unit_get_by_runtime_id(mount_runtime_id & 0xffff);
    if ((rider != 0) && (mount != 0)) {
        battle_unit_mount_rider_onto_unit(rider, mount);
        return 1;
    }
    return 0;
}
