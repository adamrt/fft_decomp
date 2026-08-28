#include "fft/battle.h"

battle_unit_misc_data_t* battle_unit_get_mount_misc_data(battle_unit_misc_data_t* unit) {
    if (unit->mount_state == BATTLE_MISC_MOUNT_STATE_RIDER) {
        return battle_unit_get_misc_data_by_misc_id(unit->mount_partner_misc_id);
    }
    return unit;
}
