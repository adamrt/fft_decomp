#include "fft/event_bunit.h"
#include "psx/types.h"

/*
 * BUNIT.OUT's per-job ability list builder, the sibling of the
 * JOBSTTS routine jobstts_ability_build_list. The two share their parameter
 * list, their category window table, their filter loop and their encode path;
 * this one additionally honours the uses_monster_skillset restriction.
 */

s32 bunit_create_ability_list(s16 unit_index, s16 job_id, s32 ability_category, s16* list, s32 mode) {
    s32 skillset;
    s16 generic_index;
    s16* abilities;
    s16* ability_cursor;
    s32 i;
    s32 count;
    s32 lo;
    s32 hi;
    s32 filter_lo;
    s32 filter_hi;
    s32 kind;
    s32 ability_id;
    s32 encoded_ability_id;
    s32 byte_offset;
    bunit_unit_data_t* unit_data;
    s16 scratch[4];

    /* Accepted spelling: bunit_job_get_skillset returns u8. The target keeps the
     * whole return word, so the call site casts to the wider signature rather than
     * re-masking. */
    skillset = ((s32 (*)(s32))bunit_job_get_skillset)(job_id);
    generic_index = bunit_job_get_generic_index(job_id);
    if (bunit_job_find_first_for_skillset(skillset) == -1) {
        skillset = g_bunit_unit_data[unit_index]->abilities[0];
    }
    abilities = (s16*)main_ability_store_skillset_abilities(skillset, SKILLSET_ABILITY_FILTER_ALL);

    if (ability_category == 0) {
        lo = 1;
        hi = ABILITY_ID_REACTION_FIRST - 1;
    } else if (ability_category == 1) {
        lo = ABILITY_ID_REACTION_FIRST;
        hi = ABILITY_ID_SUPPORT_FIRST - 1;
    } else if (ability_category == 2) {
        lo = ABILITY_ID_SUPPORT_FIRST;
        hi = ABILITY_ID_MOVEMENT_FIRST - 1;
    } else if (ability_category == 3) {
        lo = ABILITY_ID_MOVEMENT_FIRST;
        hi = ABILITY_ID_RANDOM_FIRST - 1;
    } else {
        lo = 1;
        hi = ABILITY_ID_RANDOM_FIRST - 1;
    }

    i = 0;
    filter_lo = lo;
    filter_hi = hi;
    ability_cursor = abilities;
    do {
        if (*ability_cursor < filter_lo || filter_hi < *ability_cursor) {
            *ability_cursor = 0;
        }
        ability_cursor++;
        i++;
    } while (i < SKILLSET_ABILITY_LIST_COUNT);

    count = 0;
    i = 0;
    unit_data = g_bunit_unit_data[unit_index];
    byte_offset = generic_index * 3 + (s32) & ((bunit_unit_data_t*)0)->unlocked_jobs[3];
    bunit_bits_init_reader((const u8*)(byte_offset + (s32)unit_data));
    do {
        if (bunit_job_is_special_monster(g_bunit_unit_data[unit_index]->monster_base_job_id)) {
            kind = 1;
        } else if (g_bunit_unit_data[unit_index]->uses_monster_skillset != 0) {
            if (i >= 3) {
                break;
            }
            kind = 1;
        } else {
            kind = bunit_bits_read(1);
        }

        ability_id = abilities[i];
        if (ability_id != 0) {
            encoded_ability_id = ability_id;
            /* Emits nothing; keeps the table index off the encoded copy's register. */
            __asm__("" : "=r"(ability_id) : "0"(ability_id));
            if (mode == 0) {
                if (kind == 0) {
                    if ((g_main_ability_data[ability_id].type_flags >> ABILITY_TYPE_FLAG_CANNOT_LEARN_WITH_JP_SHIFT)
                        == 0) {
                        encoded_ability_id |= ABILITY_LIST_ENTRY_DISABLED;
                    } else {
                        encoded_ability_id |= ABILITY_LIST_ENTRY_DISABLED | ABILITY_LIST_ENTRY_HIDE_DETAILS;
                    }
                }
                list[count] = encoded_ability_id;
                count++;
            } else if (mode == 3 && kind == 0) {
                count++;
            }
        }
        i++;
    } while (i < SKILLSET_ABILITY_LIST_COUNT);

    if (mode != 3) {
        list[count] = ABILITY_LIST_ENTRY_END;
    }
    return count;
}
