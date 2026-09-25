#include "fft/world.h"

/*
 * Builds a sorted, unique list of Reaction, Support, or Movement abilities
 * that a formation unit can equip.
 *
 * The list includes abilities learned in the unit's base job and generic jobs
 * Chemist through Dancer. Squire appears only when it is the base job, and
 * Mime is excluded. The -1-terminated result marks the current job's innate
 * support ability unavailable so the menu rejects it. The slot values are 1
 * for Reaction, 2 for Support, and 3 for Movement.
 */
s32 world_ability_build_equippable_rsm_list(s16 unit_index, s32 slot, s16* out) {
    s16 abilities[204];
    s32 count;
    s32 i;
    s32 offset;
    s16 scheduled_job;
    /* Preserve the original compiler's bubble-sort register allocation. */
    register s32 n __asm__("$6");
    s32 last;
    /* Preserve the original compiler's filtering-loop register allocation. */
    register s32 ability __asm__("$6");
    s16* dst;
    /* Pins required: unpinned, the -1 sentinel leaves t0 and the list pointers swap a0/v1, adding nops. */
    register s32 abilities_base __asm__("$4");
    register s32 shifted_job __asm__("$2");
    register s32 preferred_job __asm__("$7");
    s16 job_id = g_world_formation_unit_pointers[unit_index]->job_id;

    count = world_ability_find_unit_abilities(
        unit_index, world_job_get_base(unit_index), slot, abilities, WORLD_ABILITY_LIST_MODE_LEARNED_ONLY);
    for (i = 1; i < JOB_ID_GENERIC_COUNT - 1; i++) {
        count += world_ability_find_unit_abilities(
            unit_index, JOB_ID_SQUIRE + i, slot, &abilities[count], WORLD_ABILITY_LIST_MODE_LEARNED_ONLY);
    }

    n = count - 1;
    for (; n > 0; n--) {
        for (i = 0; i < n; i++) {
            s32 next = abilities[i + 1];
            s32 cur = abilities[i];
            register s32 swap __asm__("$3") = next;
            /* Loads next into v0 and copies it to v1 (move v1,v0) rather than loading v1 directly. */
            __asm__("" : "=r"(swap) : "0"(swap));
            if (cur >= next) {
                abilities[i] = swap;
                abilities[i + 1] = cur;
            }
        }
    }

    last = ABILITY_LIST_ENTRY_END;
    count = 0;
    abilities_base = (s32)abilities;
    if (abilities[0] != last) {
        scheduled_job = job_id;
        shifted_job = (u16)scheduled_job << 16;
        preferred_job = shifted_job >> 16;
        scheduled_job = preferred_job;
        /* Keeps the job's sign-extending sra ahead of the offset/dst setup. */
        __asm__("" : : "r"(preferred_job));
        offset = 0;
        dst = out;
        while (*(s16*)(offset + abilities_base) != ABILITY_LIST_ENTRY_END) {
            ability = *(s16*)(offset + abilities_base);
            if (ability != last) {
                if (scheduled_job == JOB_ID_CHEMIST && ability == ABILITY_ID_SUPPORT_THROW_ITEM) {
                    ability = ABILITY_LIST_ENTRY_DISABLED | ABILITY_ID_SUPPORT_THROW_ITEM;
                }
                if (scheduled_job == JOB_ID_MONK && ability == ABILITY_ID_SUPPORT_MARTIAL_ARTS) {
                    ability = ABILITY_LIST_ENTRY_DISABLED | ABILITY_ID_SUPPORT_MARTIAL_ARTS;
                }
                if (scheduled_job == JOB_ID_MEDIATOR && ability == ABILITY_ID_SUPPORT_MONSTER_TALK) {
                    ability = ABILITY_LIST_ENTRY_DISABLED | ABILITY_ID_SUPPORT_MONSTER_TALK;
                }
                if (scheduled_job == JOB_ID_NINJA && ability == ABILITY_ID_SUPPORT_TWO_SWORDS) {
                    ability = ABILITY_LIST_ENTRY_DISABLED | ABILITY_ID_SUPPORT_TWO_SWORDS;
                }
                *dst++ = ability;
                count++;
                /* Byte-offset reload from the array; typed indexing does not match. */
                last = *(s16*)((u8*)abilities + offset);
                /* Keeps the address add in v0 so it fills the loop branch's delay slot. */
                __asm__("" : : "r"(abilities_base));
            }
            offset += 2;
            abilities_base = (s32)abilities;
        }
    }
    out[count] = ABILITY_LIST_ENTRY_END;
    return count;
}
