#include "fft/world.h"

/* Builds the unit's known skillset list.
 *
 * Excludes its current job and Mime, and terminates the result with -1. */
s32 world_get_known_skillsets(s16 unit_id, s16* skillsets) {
    s16 jobs[24];
    s16* destination;
    s32 count;
    s32 own;
    s32 own_job;
    s32 job;
    s32 jobs_base;
    /* Pin required: unpinned, the jobs address is computed after the sign-extend and init moves instead of before them.
     */
    register s32 sentinel __asm__("$2");

    own = *(u16*)&g_world_formation_unit_pointers[unit_id]->job_id;
    world_job_build_unit_list(unit_id & 0xff, jobs, 1);
    count = 0;
    /* Keeps count's `move s2,zero` ahead of the unit_id sign-extension. */
    __asm__("" : : "r"(count));
    jobs_base = (s32)jobs;
    if (jobs[0] != -1) {
        own_job = (s16)own;
        own = 0;
        destination = skillsets;
        sentinel = -1;
        while (*(s16*)(own + jobs_base) != sentinel) {
            sentinel = own + jobs_base;
            job = *(s16*)sentinel;
            if ((own += 2, own_job != job)) {
                sentinel = JOB_ID_MIME;
                if (job != sentinel) {
                    count++;
                    *destination = world_job_get_skillset(job);
                    destination++;
                }
            }
            jobs_base = (s32)jobs;
            sentinel = -1;
        }
    }
    skillsets[count] = -1;
    return count;
}
