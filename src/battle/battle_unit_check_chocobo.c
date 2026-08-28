#include "fft/job.h"
#include "fft/main_unit.h"
#include "psx/types.h"

s32 battle_unit_check_chocobo(battle_stats_t* unit) {
    if ((u32)(unit->job_id - JOB_ID_CHOCOBO) >= JOB_ID_CHOCOBO_FAMILY_COUNT)
        return 0;
    if (main_unit_has_status_in_set(unit, MAIN_STATUS_CHECK_SET_UNMOUNTABLE) != 0)
        return 0;
    if (unit->mount_info != 0)
        return 2;
    return 1;
}
