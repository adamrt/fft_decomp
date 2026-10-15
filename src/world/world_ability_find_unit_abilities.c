#include "fft/main.h"
#include "fft/world.h"
#include "psx/types.h"

/*
 * List the abilities of `job`'s skillset for a formation unit and return the
 * number listed.
 *
 * `kind` selects the ability id range (0 action, 1 reaction, 2 support,
 * 3 movement, otherwise all). `mode` 0 lists every ability and tags unlearned
 * ones with 0x4000/0x6000; 1 lists learned ones (with kind 0 it defers to
 * world_ability_list_secondary_skillsets); 2 tags learned ones 0x1000 and marks
 * unaffordable JP abilities 0x4000; 3 only counts unlearned ones. Lists
 * other than mode 3 are -1 terminated.
 *
 * The second loop indexes `abilities` and `out` rather than walking pointers:
 * the target's cursors are the loop optimizer's reduced induction variables,
 * set after the unit-pointer address is formed.
 */
s32 world_ability_find_unit_abilities(s16 unit, s16 job, s32 kind, s16* out, s32 mode) {
    s32 skillset;
    s16 generic;
    s16* abilities;
    s16 lo;
    s16 hi;
    s32 i;
    s32 count;
    s16 jp;
    s32 learned;
    s16 id;

    if (mode == WORLD_ABILITY_LIST_MODE_LEARNED_ONLY && kind == MENU_ABILITY_CATEGORY_ACTION) {
        return world_ability_list_secondary_skillsets(unit, out);
    }
    skillset = job_get_skillset(job);
    generic = job_get_generic_index(job);
    if (job_find_first_for_skillset(skillset) == -1) {
        skillset = g_world_unit_pointers[unit]->abilities.fields.primary_skillset;
    }
    abilities = (s16*)main_ability_store_skillset_abilities(skillset, SKILLSET_ABILITY_FILTER_ALL);
    if (kind == MENU_ABILITY_CATEGORY_ACTION) {
        lo = 1;
        hi = ABILITY_ID_REACTION_FIRST - 1;
    } else if (kind == MENU_ABILITY_CATEGORY_REACTION) {
        lo = ABILITY_ID_REACTION_FIRST;
        hi = ABILITY_ID_SUPPORT_FIRST - 1;
    } else if (kind == MENU_ABILITY_CATEGORY_SUPPORT) {
        lo = ABILITY_ID_SUPPORT_FIRST;
        hi = ABILITY_ID_MOVEMENT_FIRST - 1;
    } else {
        lo = 1;
        if (kind == MENU_ABILITY_CATEGORY_MOVEMENT) {
            lo = ABILITY_ID_MOVEMENT_FIRST;
        }
        hi = ABILITY_ID_RANDOM_FIRST - 1;
    }
    for (i = 0; i < SKILLSET_ABILITY_LIST_COUNT; i++) {
        if (abilities[i] < lo || hi < abilities[i]) {
            abilities[i] = 0;
        }
    }
    count = 0;
    jp = g_world_unit_pointers[unit]->job_points.current.values[generic];
    world_bit_cursor_set_primary(g_world_unit_pointers[unit]->learned_abilities[generic]);
    for (i = 0; i < SKILLSET_ABILITY_LIST_COUNT; i++) {
        if (job_is_special_monster(g_world_unit_pointers[unit]->identity.job_id) != 0) {
            learned = 1;
        } else if (g_world_unit_pointers[unit]->uses_monster_skillset != 0) {
            if (i >= 3) {
                break;
            }
            learned = 1;
        } else {
            learned = bits_read_primary(1);
        }
        id = abilities[i];
        if (id == 0) {
            continue;
        }
        if (mode == WORLD_ABILITY_LIST_MODE_VIEW_ALL) {
            if (!learned) {
                if (!(g_main_ability_data[id].type_flags >> 7)) {
                    id |= ABILITY_LIST_ENTRY_DISABLED;
                } else {
                    id |= ABILITY_LIST_ENTRY_DISABLED | ABILITY_LIST_ENTRY_HIDE_DETAILS;
                }
            }
            out[count] = id;
            count++;
        } else if (mode == WORLD_ABILITY_LIST_MODE_LEARNED_ONLY) {
            if (learned) {
                out[count] = id;
                count++;
            }
        } else if (mode == WORLD_ABILITY_LIST_MODE_LEARN_MENU) {
            if (!learned) {
                if (g_main_ability_data[id].type_flags >> 7) {
                    continue;
                }
                if (jp < g_main_ability_data[id].jp_cost) {
                    id |= ABILITY_LIST_ENTRY_DISABLED;
                }
            } else {
                id |= ABILITY_LIST_ENTRY_LEARNED;
            }
            out[count] = id;
            count++;
        } else if (mode == WORLD_ABILITY_LIST_MODE_COUNT_UNLEARNED) {
            if (!learned) {
                count++;
            }
        }
    }
    if (mode != WORLD_ABILITY_LIST_MODE_COUNT_UNLEARNED) {
        out[count] = -1;
    }
    return count;
}
