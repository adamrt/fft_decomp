/* The base job and the all-units job lock check. */

#include "fft/bits.h"
#include "fft/data.h"
#include "fft/main.h"
#include "fft/unit.h"
#include "fft/world.h"
#include "psx/types.h"

/* The job that heads a unit's job list: a monster's own job, Squire for a
 * generic human identity, and for a special character the identity value
 * itself, which is that character's job id. */
s32 job_get_base_variant(s16 unit_id) {
    world_unit_t* unit = g_world_unit_pointers[unit_id];
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
s32 world_job_is_locked_for_all_units(s16 job_id) {
    s32 i;

    for (i = 0; i < g_world_formation_unit_count; i++) {
        if (job_is_special_monster(g_world_unit_pointers[i]->identity.job_id) == 0) {
            bits_init_secondary_reader_variant(g_world_unit_pointers[i]->unlocked_jobs);
            bits_read_secondary(job_id - JOB_ID_SQUIRE);
            if (bits_read_secondary(1) != 0) {
                return 0;
            }
        }
    }
    return 1;
}
