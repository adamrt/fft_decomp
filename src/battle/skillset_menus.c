/* Skillset ability loading for the item, throw, math skill, elemental, monster skill, draw out and charge menus, and
 * the AT list and turn order. */

#include "fft/battle_action.h"
#include "fft/battle_ai_decision.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/battle_status.h"
#include "fft/battle_unit.h"
#include "fft/battle_unit_animation.h"
#include "fft/data.h"
#include "fft/data_ability.h"
#include "fft/data_item.h"
#include "fft/data_job.h"
#include "fft/data_skillset.h"
#include "fft/data_status.h"
#include "fft/main.h"
#include "fft/main_item.h"
#include "fft/main_unit.h"
#include "fft/menu_at_list.h"
#include "fft/menu_command.h"
#include "fft/unit.h"
#include "psx/libc.h"
#include "psx/types.h"

static u8 g_battle_between_turn_pending_flags[21] = { 0 };

static s32 battle_action_set_move_act_flags(s32 idx, s32 move_flag, s32 act_flag);
static s32 battle_action_should_end_unit_turn(const unit_stats_t* unit);
static s32 battle_action_insert_at_list_entry(s32 battle_id, s32 mode, s32 sort_key, at_entry_t* list);
static s32 battle_menu_collect_math_skill_abilities_by_flags(s32 unit_id, u8 skillset, s16* out, u8 mask);
static s32 battle_menu_has_monster_skill_supporter_at_tile(s32 team_flags, s32 x, s32 y, s32 total_height);
static s32 battle_status_build_upkeep_action(s32 unit_id, unit_stats_t* unit);
static s32 battle_status_is_unit_excluded_from_at_list(const unit_stats_t* unit);

/* Build the parallel ability lists for one of a unit's skillsets.
 *
 * Every learned slot of the skillset is listed in ability_ids (terminated with
 * -1); the count is returned. For a monster, or a skillset that is neither a
 * generic job skillset nor the unit's primary or base job skillset, every slot
 * counts as learned. Ordinary abilities also report MP cost (halved, rounding up, by
 * Half of MP), CT (halved by Short Charge, zeroed by Non-Charge unless the
 * ability perseveres), a flag byte (bit 0 reflectable, bit 1 affected by
 * Silence) and the number of turns until it resolves; Charge abilities report
 * CT and turns only. A Frog may list only the Frog spell, and abilities
 * requiring a sword or Materia Blade are skipped when the unit lacks one.
 *
 * The two flag bytes need separate locals: sharing one keeps the attribute
 * byte in a globally allocated register, which the target does not. */
s32 battle_menu_get_unit_skillset_ability_data(
    s32 unit_id, u8 skillset, s16* ability_ids, u8* mp_out, u8* ct_out, s32 unused, u8* flags_out, u8* turns_out) {
    at_entry_t at_list[AT_LIST_CAPACITY];
    unit_stats_t* unit;
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
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FROG)] & STATUS_BYTE_MASK(STATUS_ID_FROG)) { /* Frog */
        frog = 1;
    }
    if (unit->ability_flags.support[1] & BATTLE_SUPPORT_SET_2_HALF_MP) {
        half_mp = 1;
    }
    if (unit->ability_flags.support[3] & BATTLE_SUPPORT_SET_4_NON_CHARGE) {
        charge = 2;
    } else if (unit->ability_flags.support[3] & BATTLE_SUPPORT_SET_4_SHORT_CHARGE) {
        charge = 1;
    }
    battle_action_calculate_at_list(at_list, BATTLE_AT_LIST_MODE_ALL);
    if (unit->equipped_flags & UNIT_EQUIPPED_FLAG_MATERIA_BLADE) {
        materia_blade = 1;
    }
    if (unit->equipped_flags & UNIT_EQUIPPED_FLAG_SWORD) {
        sword = 1;
    }
    if (unit->unit_flags & UNIT_FLAG_MONSTER) {
        skillset_index = -1;
    } else if (skillset >= SKILLSET_ID_BASIC_SKILL && skillset < SKILLSET_ID_MIMIC) {
        skillset_index = skillset - SKILLSET_ID_BASIC_SKILL;
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
    /* The learned bits run MSB first from bit 23; only the 16 action slots are listed. */
    for (i = 0; i < 16; i++) {
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
        if (ability >= 1 && ability < ABILITY_ID_ITEM_FIRST) {
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
        } else if (ability >= ABILITY_ID_CHARGE_FIRST && ability < ABILITY_ID_MATH_FIRST) {
            ct = (g_main_charge_ability_data - ABILITY_ID_CHARGE_FIRST)[ability].ct;
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

/* Load the unit's Item skillset entries through battle_menu_collect_item_abilities and return the
 * low byte of its result (the entry count). */
s32 battle_menu_load_item_abilities(s32 unit_id, s32 skillset, u8* out_item_ids) {
    return battle_menu_collect_item_abilities(unit_id, skillset & 0xFF, out_item_ids) & 0xFF;
}

/*
 * Collect the item ids of the unit's learned Item abilities into out_item_ids,
 * terminated by ITEM_ID_NONE, and return 0x100 plus the number of entries (0
 * when the unit or the skillset is wrong, or the menu is empty).
 *
 * Loading the ability id into `item_index` before subtracting ABILITY_ID_ITEM_FIRST
 * keeps the table index in its own s16 pseudo, so GCC emits the target's fresh
 * `addiu v1,-0x170` on the sign-extended 16-bit value instead of reusing the
 * masked `id + 0xfe90` sum the range test already holds (the two differ only
 * above bit 15); spelled as one expression it is one instruction short.
 */
s32 battle_menu_collect_item_abilities(s32 unit_id, u8 skillset, u8* out_item_ids) {
    unit_stats_t* unit;
    u16 abilities[0x14];
    u8 mp_costs[0x18];
    u8 ability_ct[0x18];
    u8 ability_flags[0x18];
    u8 turns[0x18];
    s32 count;
    s32 i;
    s32 found;
    u8* table;
    s16 item_index;

    found = 0;
    unit = battle_unit_get_existing_pointer(unit_id);
    if (unit == 0) {
        return 0;
    }
    if (skillset != SKILLSET_ID_ITEM) {
        *out_item_ids = ITEM_ID_NONE;
        return 0;
    }
    count = battle_menu_get_unit_skillset_ability_data(
        unit_id, SKILLSET_ID_ITEM, (s16*)abilities, mp_costs, ability_ct, 0, ability_flags, turns);
    if (count == 0) {
        *out_item_ids = ITEM_ID_NONE;
        return 0;
    }
    for (i = 0; i < count; i++) {
        if (abilities[i] >= ABILITY_ID_ITEM_FIRST && abilities[i] < ABILITY_ID_THROW_FIRST) {
            table = g_main_item_ability_item_ids;
            item_index = abilities[i];
            item_index = item_index - ABILITY_ID_ITEM_FIRST;
            found++;
            *out_item_ids = table[item_index];
            out_item_ids++;
        }
    }
    *out_item_ids = ITEM_ID_NONE;
    return found + 0x100;
}

/* The number of items battle_menu_collect_throwable_items lists in out_item_ids:
 * its result without the 0x100 marker. */
s32 battle_menu_load_throw_abilities(s32 unit_id, s32 skillset, u8* out_item_ids) {
    return battle_menu_collect_throwable_items(unit_id, skillset & 0xFF, out_item_ids) & 0xFF;
}

/* Collect the item ids the unit may throw into `out`, terminated with
 * ITEM_ID_NONE, and return 0x100 plus their count (0 when the unit or the
 * skillset is wrong or no Throw ability is learned).
 *
 * The skillset's abilities select item types through g_main_throw_ability_item_types; every item of
 * a selected type that is throwable and in stock is listed (battle_action_decrement_player_item_quantity:
 * only a player-team unit can run out).
 */
s32 battle_menu_collect_throwable_items(s32 unit_id, u8 skillset, u8* out) {
    u16 abilities[0x14];
    u8 item_types[0x14];
    u8 mp_costs[0x14];
    u8 charge_times[0x14];
    u8 ability_flags[0x14];
    u8 turn_counts[0x14];
    struct unit_stats* unit;
    s32 count;
    s32 ability_count;
    s32 i;
    s32 j;
    s32 found;
    item_data_t* item;
    u8* type_table;
    s16 throw_index;
    u8 type;
    u8 item_id;

    count = 0;
    unit = battle_unit_get_existing_pointer(unit_id);
    if (unit == 0) {
        return 0;
    }
    if (skillset != SKILLSET_ID_THROW
        || (ability_count = battle_menu_get_unit_skillset_ability_data(
                unit_id, SKILLSET_ID_THROW, (s16*)abilities, mp_costs, charge_times, 0, ability_flags, turn_counts))
            == 0) {
        *out = ITEM_ID_NONE;
        return 0;
    }
    for (i = 0; i < ability_count; i++) {
        if (abilities[i] >= ABILITY_ID_THROW_FIRST && abilities[i] < ABILITY_ID_JUMP_FIRST) {
            type_table = g_main_throw_ability_item_types;
            throw_index = abilities[i];
            throw_index = throw_index - ABILITY_ID_THROW_FIRST;
            item_types[i] = type_table[throw_index];
        }
    }
    for (i = 0; i < ITEM_ID_COUNT; i++) {
        item = &g_main_item_primary_data[i];
        type = item->type;
        found = 0;
        for (j = 0; j < ability_count; j++) {
            if (type == item_types[j]) {
                found = 1;
                break;
            }
        }
        if (found != 0 && (item->type_flags & ITEM_TYPE_FLAG_WEAPON) != 0
            && (g_main_item_weapon_data[item->secondary_data_id].flags & WEAPON_FLAG_THROWABLE) != 0) {
            item_id = i;
            if (battle_action_decrement_player_item_quantity(unit, item_id, 0) == 0) {
                *out = item_id;
                out++;
                count++;
            }
        }
    }
    *out = ITEM_ID_NONE;
    return count + 0x100;
}

/* Collect the unit's learned Math Skill attribute abilities (CT, Level, Exp,
 * Height) into out_ability_ids. */
s32 battle_menu_load_math_skill_attributes(s32 unit_id, s32 skillset, s16* out_ability_ids) {
    return battle_menu_collect_math_skill_abilities_by_flags(
        unit_id, skillset, out_ability_ids, CALCULATOR_ATTRIBUTE_MASK);
}

/* Collect the unit's learned Math Skill divisor abilities (Prime, 5, 4, 3)
 * into out_ability_ids. */
s32 battle_menu_load_math_skill_multiples(s32 unit_id, s32 skillset, s16* out_ability_ids) {
    return battle_menu_collect_math_skill_abilities_by_flags(
        unit_id, skillset, out_ability_ids, CALCULATOR_MULTIPLE_MASK);
}

/* Collect the unit's learned Math Skill abilities whose
 * g_main_math_rsm_ability_data flags intersect mask into out, -1-terminated,
 * and return the count. An empty list writes no terminator. */
static s32 battle_menu_collect_math_skill_abilities_by_flags(s32 unit_id, u8 skillset, s16* out, u8 mask) {
    u16 abilities[0x14];
    u8 mp_costs[0x18];
    u8 charge_times[0x18];
    u8 ability_flags[0x18];
    u8 turn_counts[0x18];
    s32 count;
    s32 ability_count;
    s32 i;
    s16 ability;
    u8* secondary_flags;
    u8 flag_bits;

    count = 0;
    if (battle_unit_get_existing_pointer(unit_id) == 0) {
        return 0;
    }
    if (skillset != SKILLSET_ID_MATH_SKILL) {
        *out = -1;
        return 0;
    }
    ability_count = battle_menu_get_unit_skillset_ability_data(
        unit_id, SKILLSET_ID_MATH_SKILL, (s16*)abilities, mp_costs, charge_times, 0, ability_flags, turn_counts);
    if (ability_count == 0) {
        return 0;
    }
    for (i = 0; i < ability_count; i++) {
        ability = abilities[i];
        if (ability >= ABILITY_ID_MATH_FIRST && ability < ABILITY_ID_REACTION_FIRST) {
            secondary_flags = &g_main_math_rsm_ability_data[ability - ABILITY_ID_MATH_FIRST];
            flag_bits = *secondary_flags;
            if (flag_bits & mask) {
                *out++ = ability;
                count++;
            }
        }
    }
    *out = -1;
    return count;
}

/*
 * Collect the unit's Calculator-eligible abilities from the four skillsets
 * in g_battle_menu_calculator_skillsets into out, -1-terminated, and return the
 * count. Only a request for the Math Skill skillset collects anything.
 *
 * The record pointer must be taken inside the inner loop: hoisting the table
 * base into a local before the loop swaps the two preheader address setups.
 */
s32 battle_menu_collect_calculator_abilities(s32 unit_id, u8 skillset, s16* out) {
    s16 abilities[0x14];
    u8 mp_costs[0x18];
    u8 charge_times[0x18];
    u8 ability_flags[0x18];
    u8 turn_counts[0x18];
    u8 skillsets[4];
    s32 count;
    s32 ability_count;
    s32 i;
    s32 j;
    s16 ability;
    ability_secondary_data_t* secondary;

    count = 0;
    memcpy(skillsets, g_battle_menu_calculator_skillsets, sizeof(skillsets));
    if (battle_unit_get_existing_pointer(unit_id) == 0) {
        return 0;
    }
    if (skillset != SKILLSET_ID_MATH_SKILL) {
        *out = -1;
        return 0;
    }
    for (i = 0; i < 4; i++) {
        ability_count = battle_menu_get_unit_skillset_ability_data(
            unit_id, skillsets[i], abilities, mp_costs, charge_times, 1, ability_flags, turn_counts);
        if (ability_count != 0) {
            for (j = 0; j < ability_count; j++) {
                ability = abilities[j];
                secondary = &g_main_ability_range_data[ability];
                if (secondary->flags_3 & ABILITY_SECONDARY_FLAG_3_CALCULATOR_ELIGIBLE) {
                    *out++ = ability;
                    count++;
                }
            }
        }
    }
    *out = -1;
    return count;
}

/* Return the Elemental (Geomancy) ability for the surface under the unit when
 * the unit knows and can use it, otherwise 0. */
u16 battle_menu_get_elemental_ability(s32 unit_id, u8 skillset) {
    unit_stats_t* unit;
    s16 abilities[0x14];
    u8 mp_costs[0x18];
    u8 ability_ct[0x18];
    u8 ability_flags[0x18];
    u8 turns[0x18];
    s32 count;
    s32 i;
    s16 terrain_ability;

    unit = battle_unit_get_existing_pointer(unit_id);
    if (unit == 0) {
        return 0;
    }
    if (skillset != SKILLSET_ID_ELEMENTAL) {
        return 0;
    }
    count = battle_menu_get_unit_skillset_ability_data(
        unit_id, SKILLSET_ID_ELEMENTAL, abilities, mp_costs, ability_ct, 1, ability_flags, turns);
    if (count == 0) {
        return 0;
    }
    terrain_ability
        = g_main_terrain_geomancy_abilities[g_battle_map_tile_data[battle_map_calculate_location(unit)].surface.type];
    for (i = 0; i < count; i++) {
        if (abilities[i] == terrain_ability) {
            return terrain_ability;
        }
    }
    return 0;
}

/*
 * Collect the monster-skill ability ids a unit can use into `out`, with one
 * flag byte per listed ordinary ability in `flags_out` (bit 0 reflectable, bit
 * 1 affected by Silence).
 *
 * A monster lists its first three skills, and the fourth too when a Monster
 * Skill supporter stands next to it. A Frog lists Frog Attack first and then
 * only the Frog spell. The function has no return statement and writes no
 * terminator after a list; its one caller ignores the result.
 *
 * The target keeps two live copies of the ability id: `id` in $a0 feeds both
 * sign extensions and the `addiu -1` range check, while `ability` in $a1
 * carries the value the `sh` store writes and is shared with the Frog arm's
 * Frog Attack.
 */
s32 battle_menu_collect_monster_skill_abilities(s32 unit_id, u8 skillset, s16* out, s32 unused, u8* flags_out) {
    unit_stats_t* unit;
    s32 count = 3;
    s32 frog = 0;
    s32 i;
    s16 id;
    s16 ability;
    u8 flags;

    unit = battle_unit_get_existing_pointer(unit_id);
    if (unit == 0) {
        return 0;
    }
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FROG)] & STATUS_BYTE_MASK(STATUS_ID_FROG)) {
        frog = 1;
    }
    if (g_main_action_menu_types_by_skillset[skillset] != ACTION_MENU_TYPE_MONSTER) {
        *out = -1;
        return 0;
    }
    if (frog != 0) {
        ability = ABILITY_ID_FROG_ATTACK;
        *out = ability;
        out++;
    }
    if (battle_menu_count_monster_skill_supporters(unit) != 0) {
        count = MONSTER_SKILLSET_ABILITY_COUNT;
    }
    for (i = 0; i < count; i++) {
        id = main_ability_get_id_from_skillset(skillset, i);
        ability = id;
        if ((frog == 0 || id == ABILITY_ID_BLACK_MAGIC_FROG) && id != 0 && id >= 1 && id < ABILITY_ID_ITEM_FIRST) {
            flags = g_main_ability_range_data[id].flags_3;
            *flags_out++ = (flags >> 7) | ((flags & ABILITY_SECONDARY_FLAG_3_AFFECTED_BY_SILENCE) >> 4);
            *out = ability;
            out++;
        }
    }
}

/* Monster Skill check: the number of the four tiles adjacent to the unit that
 * hold a same-team Monster Skill supporter within 5 of the unit's total
 * height. Returns 0 for an empty slot. */
s32 battle_menu_count_monster_skill_supporters(const unit_stats_t* unit) {
    battle_unit_height_data_t height_data;
    s32 team;
    s32 x;
    s32 y;
    s32 height;
    s32 sum;

    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return 0;
    }
    battle_unit_calculate_height_data(&height_data, unit->battle_id);
    team = unit->team_flags;
    x = unit->x;
    y = unit->position.y;
    height = height_data.total_height;
    sum = battle_menu_has_monster_skill_supporter_at_tile(team, x - 1, y, height)
        + battle_menu_has_monster_skill_supporter_at_tile(team, x + 1, y, height);
    sum += battle_menu_has_monster_skill_supporter_at_tile(team, x, y - 1, height);
    return sum + battle_menu_has_monster_skill_supporter_at_tile(team, x, y + 1, height);
}

/* Return whether a unit on the team of `team_flags` that has the Monster Skill
 * support ability and can react stands on the tile, with a total height within
 * 5 of `total_height` (taken as a byte). */
static s32 battle_menu_has_monster_skill_supporter_at_tile(s32 team_flags, s32 x, s32 y, s32 total_height) {
    battle_unit_height_data_t height;
    u8 reference_height;
    s32 unit_index;
    s32 unit_team_flags = team_flags;

    if (x < 0 || x >= g_battle_map_max_x) {
        return 0;
    }
    if (y >= 0) {
        if (y >= g_battle_map_max_y) {
            return 0;
        }

        unit_index = 0;
        reference_height = total_height;
        for (; unit_index < BATTLE_UNIT_SLOT_COUNT; unit_index++) {
            unit_stats_t* unit = &g_battle_unit_stats[unit_index];

            if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE || unit->x != x || unit->position.y != y
                || battle_action_is_reaction_prevented(unit) != 0
                || !(unit->ability_flags.support[3] & BATTLE_SUPPORT_SET_4_MONSTER_SKILL)
                || ((unit->team_flags ^ unit_team_flags) & UNIT_TEAM_MASK)) {
                continue;
            }
            battle_unit_calculate_height_data(&height, unit->battle_id);
            if ((reference_height - height.total_height) >= -5 && (reference_height - height.total_height) <= 5) {
                return 1;
            }
        }
    }

    return 0;
}

/* The number of katanas battle_menu_collect_draw_out_katanas lists in
 * out_item_ids: its result without the 0x100 marker. */
s32 battle_menu_load_draw_out_abilities(s32 unit_id, s32 skillset, u8* out_item_ids) {
    return battle_menu_collect_draw_out_katanas(unit_id, skillset & 0xFF, out_item_ids) & 0xFF;
}

/* Collect the katana item id of each of the unit's learned Draw Out abilities
 * into out_item_ids, terminated by ITEM_ID_NONE, and return 0x100 plus their
 * count (0 when the unit or skillset is wrong or the list is empty). Draw Out
 * abilities run in the same order as their katanas. */
s32 battle_menu_collect_draw_out_katanas(s32 unit_id, u8 skillset, u8* out_item_ids) {
    u16 abilities[0x10];
    u8 mp_costs[0x10];
    u8 ability_flags[0x10];
    u8 ability_ct[0x10];
    u8 turns[0x10];
    s32 ability_count;
    s32 i;
    s32 count;
    u32 ability_id;
    u32 item_id;

    count = 0;
    if (battle_unit_get_existing_pointer(unit_id) == 0) {
        return 0;
    }
    if (skillset != SKILLSET_ID_DRAW_OUT) {
        *out_item_ids = ITEM_ID_NONE;
        return 0;
    }
    ability_count = battle_menu_get_unit_skillset_ability_data(
        unit_id, SKILLSET_ID_DRAW_OUT, (s16*)abilities, mp_costs, ability_ct, 0, ability_flags, turns);
    if (ability_count == 0) {
        *out_item_ids = ITEM_ID_NONE;
        return 0;
    }
    for (i = 0; i < ability_count; i++) {
        ability_id = abilities[i];
        if (ability_id < ABILITY_ID_ITEM_FIRST) {
            item_id = ability_id - (ABILITY_ID_DRAW_OUT_ASURA - ITEM_ID_ASURA_KNIFE);
            *out_item_ids = item_id;
            out_item_ids++;
            count++;
        }
    }
    *out_item_ids = ITEM_ID_NONE;
    return count + 0x100;
}

/* battle_menu_load_charge_skillset for a caller that only wants the ability ids;
 * the charge times and turn counts go to scratch arrays. */
s32 battle_menu_call_load_charge_skillset(s32 unit_id, u8 skillset_id, s16* ability_ids) {
    u8 charge_times[16];
    u8 turn_counts[16];

    return battle_menu_load_charge_skillset(unit_id, skillset_id, ability_ids, charge_times, turn_counts);
}

/* Load the skillset's usable abilities with their CT and turn counts into the
 * caller's lists; the MP-cost and silence/reflect flag lists are scratch. */
s32 battle_menu_load_charge_skillset(s32 unit_id, s32 skillset, s16* abilities, u8* ability_ct, u8* turns) {
    u8 mp_costs[16];
    u8 ability_flags[16];

    return battle_menu_get_unit_skillset_ability_data(
        unit_id, skillset, abilities, mp_costs, ability_ct, 0, ability_flags, turns);
}

/* The clocktick loop between turns. RESET clears every unit's CT and turn state; FIND_ACTIVE_UNIT
 * returns 0xd000 | the first unit not Dead, Crystal, Jumping, Petrified or Treasure (0xdf00 if
 * none). Otherwise it steps the between-turn states (CT gain, the 100-CT pick, charge and status
 * countdowns, poison and regen, mimes) until one yields a battle_turn_event_e | unit id, giving up
 * with BATTLE_TURN_EVENT_NONE after 1201 steps. */
s32 battle_action_run_between_turn_control(battle_between_turn_mode_e mode) {
    unit_stats_t* unit;
    s32 result = 0;
    s32 count = 0;
    s32 i;
    s32 best;
    s32 ret;
    s32 flags;
    u16 max_ct;
    u16 speed;
    u16 ct;
    u8 value;

    if (mode == BATTLE_BETWEEN_TURN_MODE_RESET) {
        g_battle_between_turn_state = BATTLE_BETWEEN_TURN_STATE_ADD_CT;
        g_battle_turn_unit_id = -1;
        for (i = UNIT_TEAM_COUNT - 1; i >= 0; i--) {
            g_battle_team_golem[i] = 0;
        }
        g_battle_action_current_attacker.mimic_pending = 0;

        for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
            unit = &g_battle_unit_stats[i];
            unit->ct = 0;
            unit->has_turn = 0;
            unit->ability_outcome = 0;
            unit->charged_ability_ct = 0xff;
        }

        battle_ai_end_action_simulation();
        return 0xe000;
    }

    if (mode == BATTLE_BETWEEN_TURN_MODE_FIND_ACTIVE_UNIT) {
        for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
            unit = &g_battle_unit_stats[i];
            if (unit->entd_slot != BATTLE_ENTD_SLOT_NONE) {
                if (!(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)]
                        & (STATUS_BYTE_MASK(STATUS_ID_CRYSTAL) | STATUS_BYTE_MASK(STATUS_ID_DEAD)
                            | STATUS_BYTE_MASK(STATUS_ID_JUMP)))) {
                    if (!(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_PETRIFY)]
                            & (STATUS_BYTE_MASK(STATUS_ID_PETRIFY) | STATUS_BYTE_MASK(STATUS_ID_TREASURE)))) {
                        return i | 0xd000;
                    }
                }
            }
        }
        return 0xdf00;
    }

    while (result == 0) {
        switch (g_battle_between_turn_state) {
        case BATTLE_BETWEEN_TURN_STATE_ADD_CT:
            for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
                unit = &g_battle_unit_stats[i];
                flags = battle_status_calculate_turn_flags(unit);
                if (!(flags & BATTLE_TURN_STATUS_CT_FROZEN)) {
                    speed = unit->attributes[UNIT_ATTRIBUTE_SPEED];
                    if (flags & BATTLE_TURN_STATUS_SLOW) {
                        speed >>= 1;
                    } else if (flags & BATTLE_TURN_STATUS_HASTE) {
                        speed += speed >> 1;
                    }
                    ct = unit->ct + speed;
                    /* The tick caps CT at 0xFE; only Quick sets 0xFF. */
                    if (ct >= 0xff) {
                        ct = 0xfe;
                    }
                    unit->ct = ct;
                }
            }

            g_battle_between_turn_state = BATTLE_BETWEEN_TURN_STATE_PICK_UNIT;
            break;
        case BATTLE_BETWEEN_TURN_STATE_PICK_UNIT:
            max_ct = UNIT_TURN_CT_THRESHOLD - 1;
            best = 0xff;
            for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
                unit = &g_battle_unit_stats[i];
                flags = battle_status_calculate_turn_flags(unit);
                if (!(flags & BATTLE_TURN_STATUS_CT_FROZEN)) {
                    if (max_ct < unit->ct) {
                        best = i;
                        max_ct = unit->ct;
                    }
                }
            }

            if (best != 0xff) {
                unit = &g_battle_unit_stats[best];
                /* A jumping unit waits at 99 CT, ready to act once it lands; pick again. */
                if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_JUMP)] & STATUS_BYTE_MASK(STATUS_ID_JUMP)) {
                    unit->ct = UNIT_TURN_CT_THRESHOLD - 1;
                    continue;
                }

                /* A Quick target (CT 0xFF) restarts from 0; otherwise CT beyond 100 carries over. */
                if (unit->ct == 0xff) {
                    unit->ct = 0;
                } else {
                    unit->ct = max_ct % UNIT_TURN_CT_THRESHOLD;
                }

                ret = battle_status_build_upkeep_action(best, unit);
                if (ret != 0) {
                    result = best | BATTLE_TURN_EVENT_ACTION_RESULT;
                    /* A crystal or treasure (1) or an expired Death Sentence (3) ends here without
                     * a turn; only the first counts as a casualty. */
                    if (ret & 1) {
                        if (ret == 1) {
                            main_unit_increase_casualty_counters(unit);
                        }
                        return result;
                    }
                }

                /* A staged revival or other change (2) gets its turn even when the unit can't act. */
                flags = battle_status_calculate_turn_flags(unit);
                if (!(flags
                        & (BATTLE_TURN_STATUS_CT_FROZEN | BATTLE_TURN_STATUS_INCAPACITATED | BATTLE_TURN_STATUS_DEAD))
                    || ret == 2) {
                    unit->has_turn = 1;
                    unit->movement_taken = 0;
                    unit->action_taken = 0;
                    unit->transparent_removal_flag = 1;
                    if (ret == 0) {
                        result = best | BATTLE_TURN_EVENT_UNIT_READY;
                    }
                    g_battle_between_turn_resume_state = g_battle_between_turn_state;
                    g_battle_turn_unit_id = best;
                    g_battle_between_turn_state = BATTLE_BETWEEN_TURN_STATE_UNIT_TURN;
                    return result;
                }
                continue;
            }

            g_battle_between_turn_state = BATTLE_BETWEEN_TURN_STATE_TICK_CHARGES;
            break;
        case BATTLE_BETWEEN_TURN_STATE_TICK_CHARGES:
            i = 0;
            do {
                unit = &g_battle_unit_stats[i];
                flags = battle_status_calculate_turn_flags(unit);
                if (!(flags & BATTLE_TURN_STATUS_BLOCKS_WAIT_MENU_MASK)) {
                    value = unit->charged_ability_ct;
                    if (value != 0xff && value != 0) {
                        value--;
                        unit->charged_ability_ct = value;
                    }
                }
                /* clang-format off */
            } while (++i < BATTLE_UNIT_SLOT_COUNT); g_battle_between_turn_state = BATTLE_BETWEEN_TURN_STATE_RESOLVE_CHARGE;
            /* clang-format on */
            break;
        case BATTLE_BETWEEN_TURN_STATE_RESOLVE_CHARGE:
            for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
                unit = &g_battle_unit_stats[i];
                flags = battle_status_calculate_turn_flags(unit);
                if (!(flags & BATTLE_TURN_STATUS_BLOCKS_WAIT_MENU_MASK) && unit->charged_ability_ct == 0) {
                    /* A performer's song or dance starts charging again instead of ending. */
                    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_PERFORMING)]
                        & STATUS_BYTE_MASK(STATUS_ID_PERFORMING)) {
                        main_unit_copy_last_ability_ct(unit);
                        unit->charged_ability_ct = unit->ability_ct;
                    } else {
                        unit->charged_ability_ct = 0xff;
                    }
                    g_battle_between_turn_state = BATTLE_BETWEEN_TURN_STATE_PICK_UNIT_AFTER_CHARGE;
                    result = i | BATTLE_TURN_EVENT_ABILITY_READY;
                    return result;
                }
            }

            g_battle_between_turn_state = BATTLE_BETWEEN_TURN_STATE_EXPIRE_STATUSES;
            break;
        case BATTLE_BETWEEN_TURN_STATE_EXPIRE_STATUSES:
            i = 0;
            do {
                unit = &g_battle_unit_stats[i];
                if (unit->entd_slot != BATTLE_ENTD_SLOT_NONE
                    && !(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_JUMP)]
                        & STATUS_BYTE_MASK(STATUS_ID_JUMP))) {
                    battle_status_update_expiring(i);
                }
                /* clang-format off */
            } while (++i < BATTLE_UNIT_SLOT_COUNT); g_battle_between_turn_state = BATTLE_BETWEEN_TURN_STATE_COLLECT_PENDING_ACTIONS;
            /* clang-format on */
            break;
        case BATTLE_BETWEEN_TURN_STATE_COLLECT_PENDING_ACTIONS:
            /* The pending flags list the units whose expiring statuses left an
             * action result to apply; MARK_MIMES reuses them for Mimes. */
            i = 0;
            do {
                unit = &g_battle_unit_stats[i];
                g_battle_between_turn_pending_flags[i] = 0;
                if (unit->entd_slot != BATTLE_ENTD_SLOT_NONE
                    && !(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_JUMP)]
                        & STATUS_BYTE_MASK(STATUS_ID_JUMP))) {
                    g_battle_between_turn_pending_flags[i] = unit->action.attack_type;
                }
                /* clang-format off */
            } while (++i < BATTLE_UNIT_SLOT_COUNT); g_battle_between_turn_state = BATTLE_BETWEEN_TURN_STATE_RUN_PENDING_ACTIONS;
            /* clang-format on */
            break;
        case BATTLE_BETWEEN_TURN_STATE_RUN_PENDING_ACTIONS:
            for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
                if (g_battle_between_turn_pending_flags[i] != 0) {
                    g_battle_between_turn_pending_flags[i] = 0;
                    result = i | BATTLE_TURN_EVENT_ACTION_RESULT;
                    return result;
                }
            }

            g_battle_between_turn_state = BATTLE_BETWEEN_TURN_STATE_ADD_CT;
            break;
        case BATTLE_BETWEEN_TURN_STATE_PICK_UNIT_AFTER_CHARGE:
            /* Only a Quick target (CT 0xFF, above the tick's 0xFE cap) takes its turn here. */
            max_ct = 0xfe;
            best = 0xff;
            for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
                flags = battle_status_calculate_turn_flags(&g_battle_unit_stats[i]);
                if (!(flags & BATTLE_TURN_STATUS_CT_FROZEN)) {
                    if (max_ct < g_battle_unit_stats[i].ct) {
                        best = i;
                        max_ct = g_battle_unit_stats[i].ct;
                    }
                }
            }

            if (best != 0xff) {
                unit = &g_battle_unit_stats[best];
                unit->has_turn = 1;
                unit->transparent_removal_flag = 1;
                unit->ct = 0;
                unit->movement_taken = 0;
                unit->action_taken = 0;
                g_battle_between_turn_resume_state = BATTLE_BETWEEN_TURN_STATE_PICK_UNIT_AFTER_CHARGE;
                g_battle_turn_unit_id = best;
                g_battle_between_turn_state = BATTLE_BETWEEN_TURN_STATE_UNIT_TURN;
                result = best | BATTLE_TURN_EVENT_UNIT_READY;
                return result;
            }

            g_battle_between_turn_resume_state = BATTLE_BETWEEN_TURN_STATE_RESOLVE_CHARGE;
            g_battle_between_turn_state = BATTLE_BETWEEN_TURN_STATE_MARK_MIMES;
            break;
        case BATTLE_BETWEEN_TURN_STATE_UNIT_TURN:
            for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
                unit = &g_battle_unit_stats[i];
                if (unit->has_turn != 0) {
                    s32 unit_status;
                    s32 end_turn;

                    unit_status = battle_status_calculate_turn_flags(unit);
                    end_turn = battle_action_should_end_unit_turn(unit);
                    if ((unit_status & BATTLE_TURN_STATUS_CT_FROZEN) || end_turn != 0) {
                        battle_action_end_turn(i);
                        unit->has_turn = 0;
                    } else {
                        result = i | BATTLE_TURN_EVENT_UNIT_READY;
                        return result;
                    }
                }
            }

            g_battle_between_turn_state = BATTLE_BETWEEN_TURN_STATE_POISON_AND_REGEN;
            break;
        case BATTLE_BETWEEN_TURN_STATE_POISON_AND_REGEN:
            g_battle_between_turn_state = BATTLE_BETWEEN_TURN_STATE_TERRAIN_POISON;
            if (g_battle_turn_unit_id != -1) {
                unit = &g_battle_unit_stats[g_battle_turn_unit_id];
                if (battle_status_apply_poison_and_regen(unit)) {
                    result = g_battle_turn_unit_id | BATTLE_TURN_EVENT_ACTION_RESULT;
                    return result;
                }
            }
            break;
        case BATTLE_BETWEEN_TURN_STATE_TERRAIN_POISON:
            g_battle_between_turn_state = BATTLE_BETWEEN_TURN_STATE_REMOVE_TRANSPARENT;
            if (g_battle_turn_unit_id != -1) {
                unit = &g_battle_unit_stats[g_battle_turn_unit_id];
                if (battle_status_prepare_terrain_poison(unit)) {
                    result = g_battle_turn_unit_id | BATTLE_TURN_EVENT_ACTION_RESULT;
                    return result;
                }
            }
            break;
        case BATTLE_BETWEEN_TURN_STATE_REMOVE_TRANSPARENT:
            g_battle_between_turn_state = BATTLE_BETWEEN_TURN_STATE_MARK_MIMES;
            if (g_battle_turn_unit_id != -1) {
                unit = &g_battle_unit_stats[g_battle_turn_unit_id];
                if (battle_status_remove_transparent(unit)) {
                    result = g_battle_turn_unit_id | BATTLE_TURN_EVENT_ACTION_RESULT;
                    return result;
                }
            }
            break;
        case BATTLE_BETWEEN_TURN_STATE_MARK_MIMES:
            if (g_battle_action_current_attacker.mimic_pending != 0) {
                i = 0;
                do {
                    unit = &g_battle_unit_stats[i];
                    g_battle_between_turn_pending_flags[i] = 0;
                    if (unit->job_id == JOB_ID_MIME && unit->entd_slot != BATTLE_ENTD_SLOT_NONE) {
                        g_battle_between_turn_pending_flags[i] = 1;
                    }
                    /* clang-format off */
                } while (++i < BATTLE_UNIT_SLOT_COUNT); g_battle_between_turn_state = BATTLE_BETWEEN_TURN_STATE_RUN_MIMES;
                /* clang-format on */
            } else {
                g_battle_between_turn_state = g_battle_between_turn_resume_state;
            }
            break;
        case BATTLE_BETWEEN_TURN_STATE_RUN_MIMES:
            g_battle_action_current_attacker.mimic_pending = 0;
            for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
                unit = &g_battle_unit_stats[i];
                if (g_battle_between_turn_pending_flags[i] != 0 && unit->entd_slot != BATTLE_ENTD_SLOT_NONE) {
                    g_battle_between_turn_pending_flags[i] = 0;
                    ret = battle_action_set_mimic_ability(unit);
                    if (ret == 1) {
                        result = i | BATTLE_TURN_EVENT_ABILITY_READY;
                        return result;
                    }
                    if (ret == -1) {
                        result = i | BATTLE_TURN_EVENT_MIME;
                        return result;
                    }
                }
            }

            g_battle_between_turn_state = g_battle_between_turn_resume_state;
            break;
        }

        count++;
        if (count >= 1201) {
            break;
        }
    }

    return BATTLE_TURN_EVENT_NONE;
}

/*
 * Per-turn status upkeep: stage the change the unit's current status set
 * produces this tick in the unit's action record.
 *
 * Returns 2 for a Reraise or undead revival and for any other staged change
 * (Defending removed, +1 Brave for a Chicken with Brave below 10), 1 when the
 * death counter runs out and the unit becomes a crystal or treasure, 3 when
 * Death Sentence expires on a unit that is not undead, and 0 when nothing happens.
 *
 * Reraise revives with a tenth of max HP; an Undead unit whose death counter
 * runs out revives on a coin flip with 1 to max HP. Outside the execute action
 * state (previews and AI simulations) a dead unit's counter does not move.
 */
static s32 battle_status_build_upkeep_action(s32 unit_id, unit_stats_t* unit) {
    action_data_t* action = &unit->action;
    s32 roll;
    s32 mask;
    u8 counter;

    battle_action_clear_current_data(action);
    roll = rand();
    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEAD)]
            & (STATUS_BYTE_MASK(STATUS_ID_DEAD) | STATUS_BYTE_MASK(STATUS_ID_UNDEAD)))
            == STATUS_BYTE_MASK(STATUS_ID_DEAD)
        && (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_RERAISE)] & STATUS_BYTE_MASK(STATUS_ID_RERAISE))) {
        action->status_changes.removal[STATUS_BYTE_INDEX(STATUS_ID_DEAD)] = STATUS_BYTE_MASK(STATUS_ID_DEAD);
        action->status_changes.removal[STATUS_BYTE_INDEX(STATUS_ID_RERAISE)] = STATUS_BYTE_MASK(STATUS_ID_RERAISE);
        action->attack_type = ACTION_TYPE_HP_HEALING | ACTION_TYPE_STATUS_CHANGE;
        action->hp_healing = unit->max_hp / 10;
        return 2;
    }
    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEAD)] & STATUS_BYTE_MASK(STATUS_ID_DEAD))
        && g_battle_action_state == ACTION_STATE_EXECUTE) {
        if (unit->team_flags & UNIT_TEAM_FLAG_IMMORTAL) {
            return 0;
        }
        if (unit->unit_flags & (UNIT_FLAG_SAVE_FORMATION | UNIT_FLAG_LOAD_FORMATION)) {
            return 0;
        }
        /* The unit leaves on the tick its counter is already 0. */
        counter = unit->death_counter;
        counter -= 1;
        if (counter != 0xff) {
            unit->death_counter = counter;
            return 0;
        }
        if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_UNDEAD)] & STATUS_BYTE_MASK(STATUS_ID_UNDEAD))
            && (roll & 1)) {
            action->status_changes.removal[STATUS_BYTE_INDEX(STATUS_ID_DEAD)] = STATUS_BYTE_MASK(STATUS_ID_DEAD);
            action->hp_healing = ((rand() * unit->max_hp) / 0x8000) + 1;
            action->attack_type = ACTION_TYPE_HP_HEALING | ACTION_TYPE_STATUS_CHANGE;
            return 2;
        }
        /* Any set bit of the mask gives a crystal: 1 in 2 units, or 3 in 4 for a
         * unit that started player-controlled; treasure otherwise. */
        mask = 0x10;
        if (unit->initial_team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED) {
            mask = 0x110;
        }
        if (roll & mask) {
            action->status_changes.infliction[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)]
                = STATUS_BYTE_MASK(STATUS_ID_CRYSTAL);
            action->attack_type = ACTION_TYPE_STATUS_CHANGE;
            return 1;
        }
        action->status_changes.infliction[STATUS_BYTE_INDEX(STATUS_ID_TREASURE)] = STATUS_BYTE_MASK(STATUS_ID_TREASURE);
        action->attack_type = ACTION_TYPE_STATUS_CHANGE;
        return 1;
    }
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEATH_SENTENCE)]
        & STATUS_BYTE_MASK(STATUS_ID_DEATH_SENTENCE)) {
        counter = unit->status_ct[STATUS_CT_INDEX(STATUS_ID_DEATH_SENTENCE)];
        counter -= 1;
        if (counter == 0) {
            action->status_changes.removal[STATUS_BYTE_INDEX(STATUS_ID_DEATH_SENTENCE)]
                = STATUS_BYTE_MASK(STATUS_ID_DEATH_SENTENCE);
            action->attack_type = ACTION_TYPE_STATUS_CHANGE;
            if (!(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_UNDEAD)]
                    & STATUS_BYTE_MASK(STATUS_ID_UNDEAD))) {
                action->status_changes.infliction[STATUS_BYTE_INDEX(STATUS_ID_DEAD)] = STATUS_BYTE_MASK(STATUS_ID_DEAD);
                return 3;
            }
        } else {
            unit->status_ct[STATUS_CT_INDEX(STATUS_ID_DEATH_SENTENCE)] = counter;
        }
    }
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEFENDING)] & STATUS_BYTE_MASK(STATUS_ID_DEFENDING)) {
        action->status_changes.removal[STATUS_BYTE_INDEX(STATUS_ID_DEFENDING)] = STATUS_BYTE_MASK(STATUS_ID_DEFENDING);
        action->attack_type = ACTION_TYPE_STATUS_CHANGE;
    }
    if (unit->brave < 10) {
        action->brave_change = ACTION_STAT_CHANGE_INCREASE | 1;
        action->attack_type |= ACTION_TYPE_PSEUDO_STATUS;
    }
    return (action->attack_type != 0) * 2;
}

/* Returns the BATTLE_TURN_STATUS_* flags that decide how a unit takes part in the
 * turn order.
 *
 * CT_FROZEN alone for an empty slot, a unit with a status of the freeze-CT set or
 * a ridden mount; INCAPACITATED | DEAD for a dead unit; otherwise INCAPACITATED |
 * SLEEP for Sleep plus the HASTE and SLOW bits. The retail status data puts Sleep
 * in the freeze-CT set, which leaves the SLEEP result unreachable. */
s32 battle_status_calculate_turn_flags(const unit_stats_t* unit) {
    s32 status1;
    s32 status4;
    s32 status5;
    s32 flags;

    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return BATTLE_TURN_STATUS_CT_FROZEN;
    }
    if (main_unit_has_status_in_set(unit, STATUS_CHECK_SET_FREEZE_CT) != 0) {
        return BATTLE_TURN_STATUS_CT_FROZEN;
    }
    if ((unit->mount_info & UNIT_MOUNT_INFO_FLAG_MOUNT) != 0) {
        return BATTLE_TURN_STATUS_CT_FROZEN;
    }
    status1 = unit->status_sets.current[0];
    status4 = unit->status_sets.current[3];
    status5 = unit->status_sets.current[4];
    if ((status1 & STATUS_BYTE_MASK(STATUS_ID_DEAD)) != 0) {
        return BATTLE_TURN_STATUS_INCAPACITATED | BATTLE_TURN_STATUS_DEAD;
    }
    flags = -((status5 & STATUS_BYTE_MASK(STATUS_ID_SLEEP)) != 0)
        & (BATTLE_TURN_STATUS_INCAPACITATED | BATTLE_TURN_STATUS_SLEEP);
    if ((status4 & STATUS_BYTE_MASK(STATUS_ID_HASTE)) != 0) {
        flags |= BATTLE_TURN_STATUS_HASTE;
    }
    if ((status4 & STATUS_BYTE_MASK(STATUS_ID_SLOW)) != 0) {
        flags |= BATTLE_TURN_STATUS_SLOW;
    }
    return flags;
}

/* Determine whether either side has met its battle-ending condition.
 *
 * Returns -1 when the blue side is defeated, 0 when all opposing units are
 * defeated, and 1 while both sides can continue. A unit of importance 3 (whose
 * loss decides the battle) that is disabled or lost decides it at once:
 * defeat for a player-team unit, victory for any other. Otherwise the side
 * left with no unit free of the KO status set loses, the player side first. */
s32 battle_action_check_battle_outcome(void) {
    s32 player_unit_count = 0;
    s32 enemy_unit_count = 0;
    s32 unit_index;
    s32 selected;
    unit_stats_t* unit;

    for (unit_index = 0; unit_index < BATTLE_UNIT_SLOT_COUNT; unit_index++) {
        unit = &g_battle_unit_stats[unit_index];
        if ((unit->initial_team_flags & UNIT_TEAM_IMPORTANCE_MASK) == UNIT_TEAM_IMPORTANCE_MASK) {
            if (unit->existence != UNIT_EXISTENCE_NONE) {
                selected = unit->existence == UNIT_EXISTENCE_DISABLED;
                selected += main_unit_has_status_in_set(unit, STATUS_CHECK_SET_UNIT_LOST);
                if (selected != 0) {
                    if (unit->initial_team_flags & UNIT_TEAM_MASK) {
                        enemy_unit_count++;
                    } else {
                        player_unit_count++;
                    }
                }
            }
        }
    }

    if (player_unit_count != 0) {
        return -1;
    }
    if (enemy_unit_count != 0) {
        return 0;
    }

    for (unit_index = 0; unit_index < BATTLE_UNIT_SLOT_COUNT; unit_index++) {
        unit = &g_battle_unit_stats[unit_index];
        if (unit->entd_slot != BATTLE_ENTD_SLOT_NONE) {
            selected = unit->initial_team_flags & UNIT_TEAM_MASK;
            if (main_unit_has_status_in_set(unit, STATUS_CHECK_SET_KO) == 0) {
                if (selected != 0) {
                    enemy_unit_count++;
                } else {
                    player_unit_count++;
                }
            }
        }
    }

    if (player_unit_count == 0) {
        return -1;
    }
    return enemy_unit_count != 0;
}

/* Return the unit's position on a freshly built AT list.
 *
 * Returns -1 for an empty slot, -3 for a unit the AT list leaves out (Dead,
 * Crystal, Petrify, Treasure or a ridden mount) and -2 when the unit is not on
 * the list. */
s32 battle_action_find_unit_turn_index(const unit_stats_t* unit) {
    at_entry_t at_list[AT_LIST_CAPACITY];
    s32 i;
    at_entry_t* entry;
    u8 battle_id;

    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE)
        return -1;
    if (battle_status_is_unit_excluded_from_at_list(unit) != 0)
        return -3;
    battle_id = unit->battle_id;
    battle_action_calculate_at_list(at_list, BATTLE_AT_LIST_MODE_TURNS_ONLY);
    for (i = 0; i < AT_LIST_CAPACITY; i++) {
        entry = &at_list[i];
        if ((entry->unit_code & AT_UNIT_ID_MASK) == battle_id) {
            return i;
        }
    }
    return -2;
}

/* Rebuild the AT list and return the unit id of the chosen entry.
 *
 * Returns BATTLE_AT_LIST_ENTRY_EMPTY for an empty entry; a charged-action
 * entry (AT_ACTION_ENTRY) returns its unit id plus
 * BATTLE_AT_LIST_ENTRY_CHARGED_ACTION. */
s32 battle_action_get_at_list_entry_unit_id(s32 at_index) {
    at_entry_t at_list[AT_LIST_CAPACITY];
    u32 entry;
    u32 unit_id;

    battle_action_calculate_at_list(at_list, BATTLE_AT_LIST_MODE_ALL);
    entry = at_list[at_index].unit_code;
    unit_id = entry & AT_UNIT_ID_MASK;
    if (unit_id == AT_END_OF_LIST) {
        return BATTLE_AT_LIST_ENTRY_EMPTY;
    }
    if (entry & AT_ACTION_ENTRY) {
        unit_id += BATTLE_AT_LIST_ENTRY_CHARGED_ACTION;
    }
    return unit_id;
}

/* Return 1 when the unit cannot participate in the AT list. */
static s32 battle_status_is_unit_excluded_from_at_list(const unit_stats_t* unit) {
    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return 1;
    }
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)]
        & (STATUS_BYTE_MASK(STATUS_ID_CRYSTAL) | STATUS_BYTE_MASK(STATUS_ID_DEAD))) {
        return 1;
    }
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_PETRIFY)]
        & (STATUS_BYTE_MASK(STATUS_ID_PETRIFY) | STATUS_BYTE_MASK(STATUS_ID_TREASURE))) {
        return 1;
    }
    if (unit->mount_info & UNIT_MOUNT_INFO_FLAG_MOUNT) {
        return 1;
    }
    return 0;
}

/* Preview where a planned action lands on the AT list.
 *
 * Runs the attack preparation for action in the preview state, rebuilds
 * at_list and counts the turns until the action resolves, then restores the
 * unit's action data, CT, move/act flags and statuses. */
s32 battle_action_preview_at_list(unit_stats_t* unit, action_command_t* action, at_entry_t* at_list) {
    action_command_t saved_action;
    action_command_t* action_data = &unit->action_command;
    s32 old_state;
    s32 turns;
    u8 old_charged_ability_ct;
    u8 old_inflicted_status;
    u8 old_movement_taken;
    u8 old_action_taken;

    main_util_copy_action_data(action_data, &saved_action);
    old_charged_ability_ct = unit->charged_ability_ct;
    old_inflicted_status = unit->inflicted_status[0];
    old_movement_taken = unit->movement_taken;
    old_action_taken = unit->action_taken;
    old_state = g_battle_action_state;
    g_battle_action_state = ACTION_STATE_PREVIEW;
    if (battle_action_commit_attack_preparation(action) == BATTLE_ATTACK_PREPARATION_IMMEDIATE) {
        unit->charged_ability_ct = 0;
    }
    turns = battle_action_calculate_at_list(at_list, BATTLE_AT_LIST_MODE_PREVIEW);
    if (turns != -1) {
        turns = battle_action_get_number_of_turns_to_resolve(unit->battle_id, unit->charged_ability_ct, at_list);
    }
    g_battle_action_state = old_state;
    unit->movement_taken = old_movement_taken;
    unit->action_taken = old_action_taken;
    unit->inflicted_status[0] = old_inflicted_status;
    main_status_store_current(unit);
    unit->charged_ability_ct = old_charged_ability_ct;
    main_util_copy_action_data(&saved_action, action_data);
    return turns;
}

/* Rebuilds the 40-entry AT list: charged and performing actions first, then
 * each unit's next turns simulated tick by tick under Stop/Sleep, Slow and
 * Haste. Returns -1 when no entry was added.
 *
 * One u8 local holds the Sleep CT, the charging CT and the per-tick CT gain,
 * as the target's shared $v1 shows. Its early live range conflicts with $v0,
 * which keeps the gain out of $v0 so reorg can copy the join's zero-extension
 * into both Haste delay slots. The u16 wait and the value + wait order keep
 * the target's addu operand order through combine. The perform loop's key must
 * be a multi-set u32: a single-set shift temporary gets sched1's birth boost
 * and lands after the a1 load. The 0x50-byte array is the unreferenced frame
 * area the target reserves. */
s32 battle_action_calculate_at_list(at_entry_t* list, battle_at_list_mode_e mode) {
    unit_stats_t* unit;
    u8 unused[0x50];
    s32 placed;
    s32 slot;
    s32 phase_tick;
    u16 wait;
    u8 value;
    u16 tick;
    s32 turns;
    s32 slow;
    s32 haste;
    u8 speed;
    u16 ct;
    u32 remainder;
    s32 added;
    u8 current;
    u8 innate;
    u32 key;

    placed = 0;
    phase_tick = g_battle_between_turn_resume_state == BATTLE_BETWEEN_TURN_STATE_PICK_UNIT_AFTER_CHARGE;
    for (slot = 0; slot < AT_LIST_CAPACITY; slot++) {
        list[slot].unit_code = 0xff;
        list[slot].action_id_low = 0xff;
        list[slot].sort_key = 0xffff;
    }

    if (mode != BATTLE_AT_LIST_MODE_ACTIONS_ONLY) {
        /* The unit taking its turn heads the list with key 0. */
        unit = battle_unit_find_active_data_pointer();
        if (unit != 0) {
            battle_action_insert_at_list_entry(unit->battle_id, 0, 0, list);
        }
    }

    for (slot = 0; slot < BATTLE_UNIT_SLOT_COUNT; slot++) {
        unit = &g_battle_unit_stats[slot];
        if (battle_status_is_unit_excluded_from_at_list(unit) != 0) {
            continue;
        }

        /* Stop and Sleep delay every entry by their remaining CT. */
        wait = 0;
        if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_STOP)] & STATUS_BYTE_MASK(STATUS_ID_STOP)) {
            wait = unit->status_ct[STATUS_CT_INDEX(STATUS_ID_STOP)];
        }
        if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_SLEEP)] & STATUS_BYTE_MASK(STATUS_ID_SLEEP)) {
            value = unit->status_ct[STATUS_CT_INDEX(STATUS_ID_SLEEP)];
            if (wait < value) {
                wait = value;
            }
        }

        value = unit->charged_ability_ct;
        if (value != 0xff && mode != BATTLE_AT_LIST_MODE_TURNS_ONLY) {
            tick = value + wait;
            if (value != 0 || unit->has_turn == 0 || mode != BATTLE_AT_LIST_MODE_PREVIEW) {
                tick += phase_tick;
            }
            placed += battle_action_insert_at_list_entry(slot, 1, (tick << 8) & 0xff00, list);

            /* A performer repeats its song or dance every ability_ct ticks while the tick fits
             * the key's high byte. */
            if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_PERFORMING)]
                & STATUS_BYTE_MASK(STATUS_ID_PERFORMING)) {
                main_unit_copy_last_ability_ct(unit);
                for (turns = 0; turns < AT_LIST_CAPACITY - 1; turns++) {
                    tick += unit->ability_ct;
                    key = tick;
                    if (key >= 0x100) {
                        break;
                    }
                    key <<= 8;
                    placed += battle_action_insert_at_list_entry(slot, 1, key & 0xff00, list);
                }
            }
        }

        if (mode == BATTLE_AT_LIST_MODE_ACTIONS_ONLY) {
            continue;
        }

        slow = 0;
        haste = 0;
        current = unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_SLOW)];
        innate = unit->status_sets.innate[STATUS_BYTE_INDEX(STATUS_ID_SLOW)];
        if (current & STATUS_BYTE_MASK(STATUS_ID_SLOW)) {
            slow = unit->status_ct[STATUS_CT_INDEX(STATUS_ID_SLOW)];
            /* Innate Slow or Haste outlasts the simulation (0x200 ticks). */
            if (innate & STATUS_BYTE_MASK(STATUS_ID_SLOW)) {
                slow = 0x200;
            }
            slow -= wait;
            if (slow < 0) {
                slow = 0;
            }
        }
        if (current & STATUS_BYTE_MASK(STATUS_ID_HASTE)) {
            haste = unit->status_ct[STATUS_CT_INDEX(STATUS_ID_HASTE)];
            if (innate & STATUS_BYTE_MASK(STATUS_ID_HASTE)) {
                haste = 0x200;
            }
            haste -= wait;
            if (haste < 0) {
                haste = 0;
            }
        }

        speed = unit->attributes[UNIT_ATTRIBUTE_SPEED];
        /* A zero Speed would never reach 100 CT. */
        if (speed == 0) {
            speed = 1;
        }

        tick = wait;
        ct = unit->ct;
        for (turns = 0; turns < AT_LIST_CAPACITY; turns++) {
            while (ct < UNIT_TURN_CT_THRESHOLD) {
                tick++;
                value = speed;
                /* Slow takes precedence over Haste, but both count down. */
                if (slow != 0) {
                    value >>= 1;
                    slow--;
                    if (value == 0) {
                        value = 1;
                    }
                    if (haste != 0) {
                        haste--;
                    }
                } else if (haste != 0) {
                    haste--;
                    value = value * 3 >> 1;
                }
                ct += value;
                /* Give up on a unit that cannot reach 100 CT within 500 ticks. */
                if (tick >= 501) {
                    break;
                }
            }

            /* Keys hold the tick in one byte. */
            if (tick >= 0x100) {
                break;
            }

            remainder = ct;
            remainder %= UNIT_TURN_CT_THRESHOLD;
            ct = remainder;
            /* Within a tick, the unit that overshoots 100 CT further sorts first. */
            added = battle_action_insert_at_list_entry(
                slot, 0, ((tick << 8) + (UNIT_TURN_CT_THRESHOLD - remainder)) & 0xffff, list);
            if (added == 0) {
                break;
            }
            placed += added;
        }
    }

    return -(placed == 0);
}

/* Insert an entry into the 40-entry AT list ahead of the first one with a
 * larger key; the last entry drops off. Mode 0 is a unit's turn (the battle ID).
 * Mode 1 is a charged ability landing: battle ID + AT_ACTION_ENTRY with
 * the ability id's low byte in action_id_low, or battle ID + AT_ACTION_ENTRY
 * + AT_JUMP for Jump. For an ability id of 0x100 or more the code is battle
 * ID - 0x40, which as a byte is battle ID + 0xc0: the action entry plus
 * AT_ABILITY_ID_HIGH_BIT. Returns 1 when the entry was placed. */
static s32 battle_action_insert_at_list_entry(s32 battle_id, s32 mode, s32 sort_key, at_entry_t* list) {
    unit_stats_t* unit;
    s32 i;
    s32 j;
    s32 unit_code;
    s32 ability_id;
    s32 ability_code;
    s32 cmp;
    u16 key = sort_key;

    unit = &g_battle_unit_stats[battle_id];
    for (i = 0; i < AT_LIST_CAPACITY; i++) {
        cmp = key < list[i].sort_key;
        ability_code = battle_id + AT_ACTION_ENTRY;
        if (cmp) {
            for (j = AT_LIST_CAPACITY - 1; j > i; j--) {
                list[j] = list[j - 1];
            }
            unit_code = battle_id;
            list[i].sort_key = key;
            if (mode != 0) {
                unit_code = ability_code;
                if (unit->action_command.skillset_id == SKILLSET_ID_JUMP) {
                    unit_code = battle_id + (AT_ACTION_ENTRY | AT_JUMP);
                } else {
                    ability_id = unit->action_command.ability_id;
                    list[i].action_id_low = ability_id;
                    if (ability_id >= 0x100) {
                        unit_code = battle_id - AT_ACTION_ENTRY;
                    }
                }
            }
            list[i].unit_code = unit_code;
            break;
        }
    }
    return i != AT_LIST_CAPACITY;
}

/* Mark the unit's action as taken (its move flag is left as is) and end its turn if nothing is
 * left to do. */
void battle_action_set_only_action_taken(s32 unit_id) {
    battle_action_set_move_act_flags(unit_id, 0, 1);
}

/* Mark the unit's move as taken (its action flag is left as is) and end its turn if nothing is
 * left to do. */
void battle_action_set_only_movement_taken(s32 unit_id) {
    battle_action_set_move_act_flags(unit_id, 1, 0);
}

/* Mark the unit's move and/or action as taken and end its turn once nothing is
 * left to do. -1 for an absent unit, 1 when the turn ended, otherwise 0. */
static s32 battle_action_set_move_act_flags(s32 unit_id, s32 move_flag, s32 act_flag) {
    unit_stats_t* unit = &g_battle_unit_stats[unit_id];
    s32 ret;

    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE)
        return -1;
    if (move_flag != 0) {
        unit->movement_taken = 1;
    }
    if (act_flag != 0) {
        unit->action_taken = 1;
    }
    ret = battle_action_should_end_unit_turn(unit);
    if (ret != 0) {
        ret = battle_action_end_turn(unit_id);
    }
    return ret;
}

/* End the unit's turn: CT gains 20 when it neither moved nor has Don't Move,
 * and 20 when it neither acted nor has Don't Act or Performing; the result is
 * capped at 60. As the turn start already left CT % 100, a turn costs 100 CT,
 * 80 with the move or the action unused, or 60 with both unused, and never
 * leaves more than 60. Always returns 1. */
s32 battle_action_end_turn(s32 unit_id) {
    unit_stats_t* unit = &g_battle_unit_stats[unit_id];
    u16 ct;
    u8 status5;
    u8 movement_check;
    u8 action_check;
    u32 result;

    if (g_battle_unit_stats[unit_id].entd_slot == BATTLE_ENTD_SLOT_NONE)
        return 1;
    if (unit->has_turn == 0)
        return 1;
    ct = unit->ct;
    status5 = unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DONT_MOVE)];
    movement_check = unit->movement_taken | (status5 & STATUS_BYTE_MASK(STATUS_ID_DONT_MOVE));
    action_check = (status5 & STATUS_BYTE_MASK(STATUS_ID_DONT_ACT)) | unit->action_taken
        | (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_PERFORMING)] & STATUS_BYTE_MASK(STATUS_ID_PERFORMING));
    if (movement_check == 0) {
        ct += BATTLE_TURN_UNUSED_CT_REFUND;
    }
    result = ct;
    if (action_check == 0) {
        ct += BATTLE_TURN_UNUSED_CT_REFUND;
        result = ct;
    }
    if (result > BATTLE_TURN_END_CT_MAX)
        ct = BATTLE_TURN_END_CT_MAX;
    unit->ct = ct;
    unit->has_turn = 0;
    return 1;
}

/* Return 1 when the unit's turn should end: it can no longer move (moved, or
 * Don't Move) nor act (acted, or an action restriction), and it has used at
 * least one of the two. */
static s32 battle_action_should_end_unit_turn(const unit_stats_t* unit) {
    s32 cannot_act = battle_unit_get_action_restriction(unit);
    u8 no_move;
    u8 no_action;
    u8 taken;

    no_move = unit->movement_taken
        | (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DONT_MOVE)] & STATUS_BYTE_MASK(STATUS_ID_DONT_MOVE));
    no_action = unit->action_taken | cannot_act;
    taken = unit->movement_taken | unit->action_taken;
    if (no_move != 0 && no_action != 0 && taken != 0) {
        return 1;
    }
    return 0;
}

/* 1 when the unit's turn is over. A status that blocks turns
 * (battle_status_calculate_turn_flags) ends it here. */
s32 battle_action_check_change_of_turn(s32 unit_id) {
    unit_stats_t* unit = &g_battle_unit_stats[unit_id];
    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return 1;
    }
    if (!(battle_status_calculate_turn_flags(unit) & BATTLE_TURN_STATUS_BLOCKS_TURN_MASK)) {
        return unit->has_turn == 0;
    }
    unit->has_turn = 0;
    return 1;
}

/* End the unit's Charging, Jump, Defending and Performing statuses and drop its charged ability
 * (action state none). Same body as crystals_and_treasure.c's static battle_status_cancel_charge. */
void battle_status_clear_action_state(unit_stats_t* unit) {
    main_status_set_action_state(unit, MAIN_UNIT_ACTION_STATE_NONE);
}

/* Put the unit in the Defending action state, which ends Charging, Jump and Performing and drops
 * any charged ability. Used by the Defend command and the AI's simulation of it; always returns 0. */
s32 battle_status_inflict_defending_to_battle_id(s32 unit_id) {
    main_status_set_action_state(&g_battle_unit_stats[unit_id], MAIN_UNIT_ACTION_STATE_DEFENDING);
    return 0;
}

/* Marks a present unit that has a formation index for removal: the slot is
 * emptied and its existence set to pending removal. Returns -1 when the slot is
 * empty or has no formation index, else 0. No retail code calls it. */
s32 battle_unit_mark_pending_removal(s32 unit_id) {
    unit_stats_t* unit = &g_battle_unit_stats[unit_id];

    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE)
        return -1;
    if (unit->formation_index == UNIT_FORMATION_INDEX_NONE)
        return -1;
    unit->entd_slot = BATTLE_ENTD_SLOT_NONE;
    unit->existence = UNIT_EXISTENCE_PENDING_REMOVAL;
    return 0;
}
