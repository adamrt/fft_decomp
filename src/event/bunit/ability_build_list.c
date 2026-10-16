/* A unit's ability list for one job and ability category, honoring the monster skillset restriction. */

#include "fft/data_ability.h"
#include "fft/data_skillset.h"
#include "fft/event_bunit.h"
#include "fft/job.h"
#include "fft/main.h"
#include "fft/main_unit.h"
#include "fft/menu.h"
#include "fft/unit.h"
#include "psx/types.h"

/*
 * BUNIT.OUT's per-job ability list builder, the sibling of the
 * JOBSTTS routine jobstts_ability_build_list. The two share their parameter
 * list, their category window table, their filter loop and their encode path;
 * this one additionally honors the uses_monster_skillset restriction.
 */
s32 ability_build_list(s16 unit_index, s16 job_id, s32 ability_category, s16* list, s32 mode) {
    s32 skillset = job_get_skillset(job_id);
    s16 generic_index = job_get_generic_index(job_id);
    s16* abilities;
    s32 i;
    s32 count;
    s16 lo;
    s16 hi;
    s32 kind;
    s16 ability_id;
    s16 encoded_ability_id;
    s32 byte_offset;
    bunit_unit_t* unit;

    if (job_find_first_for_skillset(skillset) == -1) {
        skillset = g_bunit_unit_pointers[unit_index]->abilities[0];
    }
    abilities = (s16*)main_ability_store_skillset_abilities(skillset, SKILLSET_ABILITY_FILTER_ALL);

    if (ability_category == MENU_ABILITY_CATEGORY_ACTION) {
        lo = 1;
        hi = ABILITY_ID_REACTION_FIRST - 1;
    } else if (ability_category == MENU_ABILITY_CATEGORY_REACTION) {
        lo = ABILITY_ID_REACTION_FIRST;
        hi = ABILITY_ID_SUPPORT_FIRST - 1;
    } else if (ability_category == MENU_ABILITY_CATEGORY_SUPPORT) {
        lo = ABILITY_ID_SUPPORT_FIRST;
        hi = ABILITY_ID_MOVEMENT_FIRST - 1;
    } else if (ability_category == MENU_ABILITY_CATEGORY_MOVEMENT) {
        lo = ABILITY_ID_MOVEMENT_FIRST;
        hi = ABILITY_ID_RANDOM_FIRST - 1;
    } else {
        lo = 1;
        hi = ABILITY_ID_RANDOM_FIRST - 1;
    }

    for (i = 0; i < SKILLSET_ABILITY_LIST_COUNT; i++) {
        if (abilities[i] < lo || hi < abilities[i]) {
            abilities[i] = 0;
        }
    }

    count = 0;
    unit = g_bunit_unit_pointers[unit_index];
    /* Integer address arithmetic preserves the final ADDU operand order. */
    byte_offset = generic_index * UNIT_LEARNED_ABILITY_BYTES_PER_JOB + (s32) & ((bunit_unit_t*)0)->learned_abilities;
    bunit_bits_init_reader((const u8*)(byte_offset + (s32)unit));
    for (i = 0; i < SKILLSET_ABILITY_LIST_COUNT; i++) {
        if (job_is_special_monster(g_bunit_unit_pointers[unit_index]->job_id)) {
            kind = 1;
        } else if (g_bunit_unit_pointers[unit_index]->uses_monster_skillset != 0) {
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
            if (mode == ABILITY_LIST_MODE_VIEW_ALL) {
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
            } else if (mode == ABILITY_LIST_MODE_COUNT_UNLEARNED && kind == 0) {
                count++;
            }
        }
    }

    if (mode != ABILITY_LIST_MODE_COUNT_UNLEARNED) {
        list[count] = ABILITY_LIST_ENTRY_END;
    }
    return count;
}
