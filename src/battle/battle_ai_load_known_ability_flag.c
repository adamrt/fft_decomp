#include "fft/battle_ai.h"
#include "fft/data.h"

/* Whether the unit has learned the given ability bit of a skillset: returns 1
 * for monsters (every ability known) and for skillsets the unit does not own.
 * Job skillsets 5..0x17 map to learned_abilities rows 0..0x12; the unit's own
 * primary or base-job skillset maps to row 0. */
s32 battle_ai_load_known_ability_flag(s32 unit_id, s32 skillset_id, s32 bit) {
    battle_stats_t* unit = &g_battle_unit_stats[unit_id];
    s32 byte;
    u8* row;

    if ((unit->unit_flags & UNIT_FLAG_MONSTER) != 0) {
        return 1;
    }
    if ((u32)(skillset_id - SKILLSET_ID_BASIC_SKILL) < (SKILLSET_ID_MIMIC - SKILLSET_ID_BASIC_SKILL)) {
        skillset_id = skillset_id - SKILLSET_ID_BASIC_SKILL;
    } else {
        if (skillset_id != unit->primary_skillset && skillset_id != unit->base_job_skillset)
            return 1;
        skillset_id = 0;
    }
    /* Typed indexing moves the field offset ahead of the unit-base addition.
     * Keep the target's address order, deriving the offset from the field. */
    row = (u8*)(skillset_id * 3 + (u32)unit + ((u32)unit->learned_abilities - (u32)unit));
    byte = bit / 8;
    return (row[byte] & (0x80 >> (bit - (byte * 8)))) != 0;
}
