#include "fft/main.h"
#include "fft/world.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/types.h"

void world_ability_learn(s16 unit_id, s16 job_id, s16 ability_id) {
    s32 skillset;
    s16 generic_job;
    s16* list;
    s32 i;
    s32 byte;
    s32 bit;
    s32 mask;
    u8* learned_abilities;

    skillset = job_get_skillset(job_id);
    generic_job = job_get_generic_index(job_id);
    list = (s16*)main_ability_store_skillset_abilities(skillset, SKILLSET_ABILITY_FILTER_ALL);
    i = 0;
    while (list[i] != ability_id) {
        i++;
    }
    byte = i >> 3;
    bit = 7 - (i & 7);
    mask = 1;
    i = mask << bit;
    /* Keeping the job-byte stride before the field address preserves the retail load/add order. */
    learned_abilities = ((world_unit_t*)((u8*)g_world_unit_pointers[unit_id] + generic_job * 3))->learned_abilities[0];
    learned_abilities[byte] |= i;
}
