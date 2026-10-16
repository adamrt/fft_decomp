/* The base job and the all-units job lock check. */

#include "fft/bits.h"
#include "fft/data_job.h"
#include "fft/job.h"
#include "fft/unit_identity.h"
#include "fft/world_formation.h"
#include "psx/types.h"

/* The job that heads a unit's job list: a monster's own job, Squire for a
 * generic human identity, and for a special character the identity value
 * itself, which is that character's job id. */
s32 job_list_get_base(s16 unit_id) {
    formation_unit_t* unit = g_formation_unit_pointers[unit_id];
    u32 job = unit->sprite_set;

    if (job == CHARACTER_IDENTITY_MONSTER) {
        job = unit->identity.job_id;
    } else if (job >= CHARACTER_IDENTITY_SELECTOR_FIRST) {
        job = JOB_ID_SQUIRE;
    }
    return job;
}

/* Returns 1 when no formation unit (special monsters aside) has generic job
 * `job_id` unlocked. */
s32 formation_job_is_locked_for_all(s16 job_id) {
    s32 i;

    for (i = 0; i < g_formation_unit_count; i++) {
        if (job_is_special_monster(g_formation_unit_pointers[i]->identity.job_id) == 0) {
            /* One bit per generic job, Squire first: skip to this job's bit. */
            bits_init_secondary_reader(g_formation_unit_pointers[i]->unlocked_jobs);
            bits_read_secondary(job_id - JOB_ID_SQUIRE);
            if (bits_read_secondary(1) != 0) {
                return 0;
            }
        }
    }
    return 1;
}
