#include "fft/bunit.h"
#include "fft/job.h"
#include "psx/types.h"

s32 bunit_job_get_base(s16 unit_id) {
    bunit_unit_data_t* unit = g_bunit_unit_data[unit_id];
    s32 job_id = unit->character_identity;

    if (job_id == CHARACTER_IDENTITY_MONSTER) {
        job_id = unit->monster_base_job_id;
    } else if ((u32)job_id >= CHARACTER_IDENTITY_SELECTOR_FIRST) {
        job_id = JOB_ID_SQUIRE;
    }
    return job_id;
}
