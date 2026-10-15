#include "fft/battle_action.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* Whether a unit is a chocobo that can be ridden: BATTLE_CHOCOBO_MOUNT_NONE for
 * any other job or a chocobo with a status of the unmountable set, PAIRED when
 * its mount_info is set, else FREE. */
battle_chocobo_mount_state_e battle_unit_get_chocobo_mount_state(const unit_stats_t* unit) {
    if (unit->job_id < JOB_ID_CHOCOBO || unit->job_id > JOB_ID_RED_CHOCOBO)
        return BATTLE_CHOCOBO_MOUNT_NONE;
    if (main_unit_has_status_in_set(unit, MAIN_STATUS_CHECK_SET_UNMOUNTABLE) != 0)
        return BATTLE_CHOCOBO_MOUNT_NONE;
    if (unit->mount_info != 0)
        return BATTLE_CHOCOBO_MOUNT_PAIRED;
    return BATTLE_CHOCOBO_MOUNT_FREE;
}
