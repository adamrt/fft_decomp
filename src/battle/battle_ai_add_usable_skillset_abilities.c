#include "fft/battle.h"

/* Append a skillset's usable abilities to the unit's AI ability list and
 * return the updated count.
 *
 * Throw adds three shuriken and three ball entries, then at most one entry for
 * the first other known throw ability; Arithmeticks adds a single entry;
 * Charge is skipped with a gun equipped. Other skillsets add each AI-usable,
 * known ability whose MP cost, Faith and sword/materia-blade requirements
 * pass. Jump keeps only the best level-jump ability, falling back to one
 * bare Jump entry. For monster skillsets only the fourth ability keeps the
 * monster-skill flag.
 *
 * The known-throw tail sits before the main body because the target emits it
 * there and GCC 2.6.3 keeps source block order. The per-loop temporaries,
 * ai as a local and the int max_mp reproduce the target's allocation, loop
 * set-up order and signed comparison. */
s32 battle_ai_add_usable_skillset_abilities(s32 unit_id, s32 skillset, s32 count) {
    s32 jump_found;
    battle_ai_data_t* ai;
    u16* abilities;
    s32 menu;
    s32 list_id;
    s32 i;
    battle_ai_ability_entry_t* entry;
    battle_ai_ability_entry_t* slot;
    battle_ai_ability_entry_t* row;
    battle_stats_t* unit;
    s32 ability_id;
    s32 unit_bits;
    u8 mp_cost;
    s32 max_mp;
    u8 flags;

    ai = &g_battle_ai_data_base;
    if (skillset == 0) {
        return count;
    }
    jump_found = 0;
    abilities = main_ability_store_skillset_abilities(skillset, SKILLSET_ABILITY_FILTER_ACTION);
    menu = g_main_action_menu_types_by_skillset[skillset];
    list_id = ai->unit_battle_ids[unit_id];
    if (menu == ACTION_MENU_TYPE_WEAPON_INVENTORY) {
        if (battle_ai_load_known_ability_flag(unit_id, skillset, 0)) {
            unit_bits = unit_id << 10;
            for (i = 0, slot = ai->ability_lists[list_id]; i < 3; i++) {
                slot[count].skillset_flags.bytes.skillset = skillset;
                slot[count].id.packed_id = unit_bits + i + ITEM_ID_THROWABLE_FIRST;
                count++;
            }
        }
        if (battle_ai_load_known_ability_flag(unit_id, skillset, 1)) {
            i = 0;
            unit_bits = unit_id << 10;
            for (row = ai->ability_lists[list_id]; i < 3; i++) {
                row[count].skillset_flags.bytes.skillset = skillset;
                row[count].id.packed_id = unit_bits + i + 0x7d;
                count++;
            }
        }
        for (i = 2; i < 12; i++) {
            if (battle_ai_load_known_ability_flag(unit_id, skillset, i)) {
                entry = &ai->ability_lists[list_id][count];
                count++;
                entry->skillset_flags.bytes.skillset = skillset;
                entry->id.packed_id = unit_id << 10;
                break;
            }
        }
        return count;
    }
    if (menu == ACTION_MENU_TYPE_ARITHMETICKS) {
        entry = &ai->ability_lists[list_id][count];
        entry->skillset_flags.bytes.skillset = skillset;
        entry->id.packed_id = unit_id << 10;
        return count + 1;
    }
    if (menu == ACTION_MENU_TYPE_CHARGE) {
        i = ai->unit_weapon_data[list_id].bytes.weapon_id;
        if (i >= 0x47 && i < 0x4d) {
            return count;
        }
    }
    unit = &g_battle_unit_stats[unit_id];
    for (i = 0; i < 16; i++) {
        ability_id = abilities[i];
        if (ability_id == 0) {
            continue;
        }
        entry = &ai->ability_lists[list_id][count];
        if (!(g_main_ability_data[ability_id].ai_flags.bytes[3] & 0x80)) {
            continue;
        }
        if (ability_id < ABILITY_ID_ITEM_FIRST) {
            mp_cost = g_main_ability_range_data[ability_id].mp_cost;
            if (unit->support_abilities[1] & BATTLE_SUPPORT_SET_2_HALF_MP) {
                mp_cost >>= 1;
            }
            max_mp = unit->max_mp;
            if (max_mp < mp_cost) {
                continue;
            }
            if ((ai->considered_ability.ai_flags.word & 0x400) && (unit->status_sets.innate[4] & 0x40)) {
                continue;
            }
            flags = g_main_ability_range_data[ability_id].flags_4;
            if (flags & 8) {
                if (!(unit->equipped_flags & 8)) {
                    continue;
                }
            } else if (flags & 4) {
                if (!(unit->equipped_flags & 4)) {
                    continue;
                }
            }
        }
        if (!battle_ai_load_known_ability_flag(unit_id, skillset, i)) {
            continue;
        }
        if (skillset >= SKILLSET_ID_MONSTER_FIRST) {
            if (i == 3) {
                entry->skillset_flags.packed |= 0x800;
            } else {
                entry->skillset_flags.packed &= ~0x800;
            }
        }
        entry->skillset_flags.bytes.skillset = skillset;
        entry->id.packed_id = ability_id;
        if (menu == ACTION_MENU_TYPE_JUMP) {
            if (ability_id >= 0x18f) {
                continue;
            }
            if (jump_found == 1) {
                count--;
                entry = &ai->ability_lists[list_id][count];
                entry->id.packed_id = ability_id;
            }
            jump_found = 1;
        }
        count++;
        entry->id.packed_id += unit_id << 10;
    }
    if (menu == ACTION_MENU_TYPE_JUMP && jump_found == 0) {
        entry->skillset_flags.bytes.skillset = SKILLSET_ID_JUMP;
        entry->id.packed_id = unit_id << 10;
    }
    return count;
}
