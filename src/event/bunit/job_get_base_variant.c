/* The job that heads a unit's job list. */

#include "fft/data.h"
#include "fft/event_bunit.h"
#include "fft/unit.h"
#include "psx/types.h"

/* The job that heads a unit's job list: a monster's own job, Squire for a
 * generic human identity, and for a special character the identity value
 * itself, which is that character's job id. */
s32 job_get_base_variant(s16 unit_id) {
    bunit_unit_t* unit = g_bunit_unit_pointers[unit_id];
    u32 job_id = unit->character_identity;

    if (job_id == CHARACTER_IDENTITY_MONSTER) {
        job_id = unit->job_id;
    } else if (job_id >= CHARACTER_IDENTITY_SELECTOR_FIRST) {
        job_id = JOB_ID_SQUIRE;
    }
    return job_id;
}
