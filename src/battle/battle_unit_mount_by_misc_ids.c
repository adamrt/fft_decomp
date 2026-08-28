#include "fft/battle.h"

s32 battle_unit_mount_by_misc_ids(u32 rider_misc_id, u32 mount_misc_id) {
    battle_unit_misc_data_t* rider;
    battle_unit_misc_data_t* mount;

    rider = battle_unit_get_misc_data_by_misc_id(rider_misc_id & 0xffff);
    mount = battle_unit_get_misc_data_by_misc_id(mount_misc_id & 0xffff);
    if ((rider != 0) && (mount != 0)) {
        battle_unit_mount_rider_onto_unit(rider, mount);
        return 1;
    }
    return 0;
}
