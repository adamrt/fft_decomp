#ifndef FORMATION_JOB_H
#define FORMATION_JOB_H

/* Formation job changes, the job wheel and its job-change animation. */

#include "psx/types.h"

/* job change */
s32 formation_job_change(s16 unit_id, s16 job_id, s32 update_equipment);

/* job wheel */
/* An entry of the job list job_list_build writes. */
enum {
    FORMATION_JOB_LIST_ENTRY_ID_MASK = 0x3ff,
    /* Set in mode INCLUDE_OTHER_UNIT_UNLOCKS on the unit's current job and on
     * a job only another unit has unlocked: the job wheel shows the entry
     * but does not change to it. */
    FORMATION_JOB_LIST_ENTRY_UNAVAILABLE = 0x4000
};

/* The one mode job_list_build tests; its callers also pass 0 and 1. */
enum { FORMATION_JOB_LIST_MODE_INCLUDE_OTHER_UNIT_UNLOCKS = 2 };

extern s8 g_formation_job_wheel_initialized;
void formation_job_menu_update(void);

#endif
