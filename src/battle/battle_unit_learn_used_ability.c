#include "fft/job.h"
#include "fft/main_unit.h"
#include "psx/types.h"

/* Integer address arithmetic preserves the retail instruction order here. */
#define LEARNED_ABILITIES_OFFSET ((u32) & ((battle_stats_t*)0)->learned_abilities)

void battle_unit_learn_used_ability(battle_stats_t* unit) {
    u8 learned[3];
    u8 skillset;
    s32 job;
    s32 i;
    s32 byte_idx;
    s32 mask;
    u8* src;
    u8* dst;
    u8* base;
    u8* learned_byte;

    job = unit->job_id;
    skillset = unit->primary_skillset;
    if (job < JOB_ID_SQUIRE) {
        job = 0;
    } else {
        job -= JOB_ID_SQUIRE;
    }
    /* Three learned-ability bytes per job, from learned_abilities (0x99). */
    dst = learned;
    src = (u8*)(((job * 3) + (s32)unit) + LEARNED_ABILITIES_OFFSET);
    do {
        *dst = *src;
        dst += 1;
        src += 1;
    } while ((s32)dst < (s32)&learned[3]);
    i = 0;
    base = (u8*)(((job * 3) + (s32)unit) + LEARNED_ABILITIES_OFFSET);
    do {
        byte_idx = i / 8;
        mask = 0x80 >> (i - (byte_idx * 8));
        if (!(learned[byte_idx] & mask)) {
            if ((u16)main_ability_get_id_from_skillset(skillset, i) == g_battle_acting_unit_used_ability_id) {
                learned_byte = base + byte_idx;
                *learned_byte = mask | *learned_byte;
            }
        }
        i += 1;
    } while (i < SKILLSET_ABILITY_LIST_COUNT);
}
