#include "fft/data.h"
#include "fft/job.h"

s16 world_job_get_base(s16 formation_idx) {
    world_formation_unit_t* unit = g_world_formation_unit_pointers[formation_idx];
    u32 job = unit->sprite_set;
    if (job == CHARACTER_IDENTITY_MONSTER) {
        job = unit->job_id;
    } else if (job >= CHARACTER_IDENTITY_SELECTOR_FIRST) {
        job = JOB_ID_SQUIRE;
    }
    return (s16)job;
}
