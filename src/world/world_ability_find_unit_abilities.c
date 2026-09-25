#include "fft/world.h"
#include "psx/types.h"

/*
 * List the abilities of `job`'s skillset for a formation unit and return the
 * number listed.
 *
 * `kind` selects the ability id range (0 action, 1 reaction, 2 support,
 * 3 movement, otherwise all). `mode` 0 lists every ability and tags unlearned
 * ones with 0x4000/0x6000; 1 lists learned ones (with kind 0 it defers to
 * world_get_known_skillsets); 2 tags learned ones 0x1000 and marks
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

    if (mode == 1 && kind == 0) {
        return world_get_known_skillsets(unit, out);
    }
    skillset = world_job_get_skillset(job);
    /* The definition's u16 parameter and s16 return conversions would change this call's codegen. */
    generic = ((s16 (*)(s32))world_job_get_generic_index)(job);
    if (world_job_find_by_skillset(skillset) == -1) {
        skillset = (u16)g_world_formation_unit_pointers[unit]->primary_skillset;
    }
    abilities = (s16*)main_ability_store_skillset_abilities(skillset, 0xF);
    if (kind == 0) {
        lo = 1;
        hi = 0x1A5;
    } else if (kind == 1) {
        lo = 0x1A6;
        hi = 0x1C5;
    } else if (kind == 2) {
        lo = 0x1C6;
        hi = 0x1E5;
    } else {
        lo = 1;
        if (kind == 3) {
            lo = 0x1E6;
        }
        hi = 0x1FD;
    }
    for (i = 0; i < 24; i++) {
        if (abilities[i] < lo || hi < abilities[i]) {
            abilities[i] = 0;
        }
    }
    count = 0;
    jp = g_world_formation_unit_pointers[unit]->job_points[generic];
    world_bit_cursor_set_primary(&g_world_formation_unit_pointers[unit]->learned_abilities[generic * 3]);
    for (i = 0; i < 24; i++) {
        if (world_job_is_special_monster(g_world_formation_unit_pointers[unit]->job_id) != 0) {
            learned = 1;
        } else if (g_world_formation_unit_pointers[unit]->uses_monster_skillset != 0) {
            if (i >= 3) {
                break;
            }
            learned = 1;
        } else {
            learned = world_bit_cursor_read_primary(1);
        }
        id = abilities[i];
        if (id == 0) {
            continue;
        }
        if (mode == 0) {
            if (!learned) {
                if (!(g_main_ability_data[id].type_flags >> 7)) {
                    id |= 0x4000;
                } else {
                    id |= 0x6000;
                }
            }
            out[count] = id;
            count++;
        } else if (mode == 1) {
            if (learned) {
                out[count] = id;
                count++;
            }
        } else if (mode == 2) {
            if (!learned) {
                if (g_main_ability_data[id].type_flags >> 7) {
                    continue;
                }
                if (jp < g_main_ability_data[id].jp_cost) {
                    id |= 0x4000;
                }
            } else {
                id |= 0x1000;
            }
            out[count] = id;
            count++;
        } else if (mode == 3) {
            if (!learned) {
                count++;
            }
        }
    }
    if (mode != 3) {
        out[count] = -1;
    }
    return count;
}
