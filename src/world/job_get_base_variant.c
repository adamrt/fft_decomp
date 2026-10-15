#include "fft/world.h"

/* The job that heads a unit's job list: a monster's own job, Squire for a
 * generic human identity, and for a special character the identity value
 * itself, which is that character's job id. */
s32 job_get_base_variant(s16 formation_idx) {
    world_unit_t* unit = g_world_unit_pointers[formation_idx];
    u32 job = unit->sprite_set;
    if (job == CHARACTER_IDENTITY_MONSTER) {
        job = unit->identity.job_id;
    } else if (job >= CHARACTER_IDENTITY_SELECTOR_FIRST) {
        job = JOB_ID_SQUIRE;
    }
    return job;
}
