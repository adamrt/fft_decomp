#ifndef FORMATION_JOB_H
#define FORMATION_JOB_H

/* The Formation screen's job menu in WORLD: changing a unit's job, the job wheel and its job-change animation; the
 * ATTACK and REQUIRE overlays' party portrait job record sits here too. */

#include "fft/job.h"
#include "psx/types.h"

/* job change */
s32 formation_job_change(s16 unit_id, s16 job_id, s32 update_equipment);
s32 formation_job_is_locked_for_all(s16 job_id);

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
void formation_job_wheel_menu(void);

/* party portraits */
/* The job record the ATTACK and REQUIRE party portrait builders last looked
 * up; each overlay defines its own. */
extern job_data_t* g_formation_job_data;

#endif
