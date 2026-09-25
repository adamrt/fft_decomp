#include "fft/world.h"
#include "psx/types.h"

/* Mark an ability as learned in the unit's job-specific bitmap.
 *
 * The filtered skillset position determines the bitmap byte and bit.
 */
void world_ability_learn(s16 unit_id, s16 job_id, s16 ability_id) {
    s32 skillset;
    s16 generic_job;
    s16* list;
    s32 i;
    s32 byte;
    s32 bit;
    s32 mask;
    u8* learned_abilities;

    skillset = world_job_get_skillset(job_id);
    generic_job = ((s16 (*)())world_job_get_generic_index)(job_id);
    list = (s16*)main_ability_store_skillset_abilities(skillset, SKILLSET_ABILITY_FILTER_ALL);
    i = 0;
    while (list[i] != ability_id) {
        i++;
    }
    byte = i >> 3;
    bit = 7 - (i & 7);
    mask = 1;
    i = mask << bit;
    /* Keeping the field base as byte arithmetic preserves the target's
     * separate addiu 0x7a before the byte-index addition. */
    learned_abilities = (u8*)g_world_formation_unit_pointers[unit_id] + generic_job * 3 + 0x7A;
    learned_abilities[byte] |= i;
}
