#include "fft/ability_flags.h"
#include "fft/battle.h"
#include "fft/data.h"
#include "fft/main_runtime.h"
#include "fft/main_unit.h"
#include "psx/types.h"

/* Build the parallel ability lists for one of a unit's skillsets.
 *
 * Every learned slot of the skillset is listed in ability_ids (terminated with
 * -1); the count is returned. Ordinary abilities also report MP cost (halved
 * by Half of MP), CT (halved by Short Charge, zeroed by Non-Charge unless the
 * ability perseveres), reflectable/silence flags and the number of turns until
 * it resolves; Charge abilities (0x196-0x19d) report CT and turns only. A Frog
 * may list only Frog (0x1d), and abilities requiring a sword or Materia Blade
 * are skipped when the unit lacks one.
 *
 * The two flag bytes need separate locals: sharing one keeps the attribute
 * byte in a globally allocated register, which the target does not. */
s32 battle_menu_get_unit_skillset_ability_data(
    s32 unit_id, u8 skillset, s16* ability_ids, u8* mp_out, u8* ct_out, s32 unused, u8* flags_out, u8* turns_out) {
    battle_at_entry_t at_list[40];
    battle_stats_t* unit;
    ability_secondary_data_t* secondary;
    s32 known;
    s16 ability;
    s32 materia_blade;
    s32 sword;
    s32 charge;
    s32 half_mp;
    s32 frog;
    s32 count;
    s32 i;
    s32 skillset_index;
    s32 mp;
    s32 ct;
    s32 kind;
    u8 requirements;
    u8 attributes;
    s32 turns;

    count = 0;
    materia_blade = 0;
    sword = 0;
    charge = 0;
    half_mp = 0;
    frog = 0;
    unit = battle_unit_get_existing_pointer(unit_id);
    if (unit == 0) {
        return 0;
    }
    if (unit->status_sets.current[2] & 2) { /* Frog */
        frog = 1;
    }
    if (unit->support_abilities[1] & BATTLE_SUPPORT_SET_2_HALF_MP) {
        half_mp = 1;
    }
    if (unit->support_abilities[3] & BATTLE_SUPPORT_SET_4_NON_CHARGE) {
        charge = 2;
    } else if (unit->support_abilities[3] & BATTLE_SUPPORT_SET_4_SHORT_CHARGE) {
        charge = 1;
    }
    battle_action_calculate_at_list(at_list, 0);
    if (unit->equipped_flags & BATTLE_UNIT_EQUIPPED_FLAG_MATERIA_BLADE) {
        materia_blade = 1;
    }
    if (unit->equipped_flags & BATTLE_UNIT_EQUIPPED_FLAG_SWORD) {
        sword = 1;
    }
    if (unit->unit_flags & UNIT_FLAG_MONSTER) {
        skillset_index = -1;
    } else if (skillset >= 5 && skillset < 0x18) {
        skillset_index = skillset - 5;
    } else if (skillset == unit->primary_skillset || skillset == unit->base_job_skillset) {
        skillset_index = 0;
    } else {
        skillset_index = -1;
    }
    if (skillset_index == -1) {
        known = 0xffffff;
    } else {
        known = main_ability_get_known(unit, skillset_index);
    }
    for (i = 0; i < 0x10; i++) {
        if (!(known & (0x800000 >> i))) {
            continue;
        }
        ability = main_ability_get_id_from_skillset(skillset, i);
        if (frog && ability != ABILITY_ID_BLACK_MAGIC_FROG) {
            continue;
        }
        if (ability == 0) {
            continue;
        }
        if (ability >= 1 && ability < 0x170) {
            secondary = &g_main_ability_range_data[ability];
            requirements = secondary->flags_4;
            if ((requirements & ABILITY_SECONDARY_FLAG_4_REQUIRES_SWORD) && !sword) {
                continue;
            }
            if ((requirements & ABILITY_SECONDARY_FLAG_4_REQUIRES_MATERIA_BLADE) && !materia_blade) {
                continue;
            }
            mp = secondary->mp_cost;
            if (half_mp) {
                mp = (mp + 1) >> 1;
            }
            ct = secondary->ct & 0x7f;
            attributes = secondary->flags_3;
            kind = (attributes >> 7) | ((attributes & ABILITY_SECONDARY_FLAG_3_AFFECTED_BY_SILENCE) >> 4);
            if (!(attributes & ABILITY_SECONDARY_FLAG_3_PERSEVERE)) {
                if (charge == 1) {
                    ct = (ct + 1) >> 1;
                }
                if (charge == 2) {
                    ct = 0;
                }
            }
            turns = battle_action_get_number_of_turns_to_resolve(unit_id, ct, at_list);
            mp_out[count] = mp;
            ct_out[count] = ct;
            flags_out[count] = kind;
            turns_out[count] = turns;
        } else if (ability >= 0x196 && ability < 0x19e) {
            ct = g_main_jump_charge_ability_data_by_ability_id[ability * 2];
            turns = battle_action_get_number_of_turns_to_resolve(unit_id, ct, at_list);
            mp_out[count] = 0;
            ct_out[count] = ct;
            flags_out[count] = 0;
            turns_out[count] = turns;
        }
        *ability_ids++ = ability;
        count++;
    }
    *ability_ids = -1;
    return count;
}
