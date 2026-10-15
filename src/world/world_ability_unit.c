#include "fft/main.h"
#include "fft/world.h"

/* Builds the unit's known skillset list.
 *
 * Excludes its current job and Mime, and terminates the result with -1. */
s32 world_ability_list_secondary_skillsets(s16 unit_id, s16* skillsets) {
    s16 jobs[24];
    s32 count;
    s32 i;
    s32 job;
    s16 own_job;

    own_job = g_world_unit_pointers[unit_id]->identity.job_id;
    world_job_build_unit_list(unit_id, jobs, 1);
    count = 0;
    for (i = 0; jobs[i] != -1; i++) {
        job = jobs[i];
        if (own_job != job && job != JOB_ID_MIME) {
            skillsets[count++] = job_get_skillset(job);
        }
    }
    skillsets[count] = -1;
    return count;
}

s32 world_ability_has_two_swords(s16 unit_id) {
    return g_world_unit_pointers[unit_id]->support_abilities[2] & BATTLE_SUPPORT_SET_3_TWO_SWORDS;
}

s32 world_ability_has_two_hands(s16 unit_id) {
    return g_world_unit_pointers[unit_id]->support_abilities[2] & BATTLE_SUPPORT_SET_3_TWO_HANDS;
}

s32 world_ability_has_secret_hunt(s32 unit_id) {
    return g_world_unit_pointers[(s16)unit_id]->support_abilities[2] & BATTLE_SUPPORT_SET_3_SECRET_HUNT;
}
