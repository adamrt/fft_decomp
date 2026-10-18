/* Build usable ability lists for the battle skillset menus, applying learned
 * abilities, equipment requirements and support abilities. The lists carry
 * the MP costs and charge timing displayed when choosing an action. */

#include "fft/ability.h"
#include "fft/at_list.h"
#include "fft/battle_map.h"
#include "fft/command.h"
#include "fft/item.h"
#include "fft/reaction.h"
#include "fft/skillset.h"
#include "fft/status.h"
#include "fft/unit.h"
#include "fft/unit_runtime.h"
#include "psx/libc.h"
#include "psx/types.h"

extern u8 g_skillset_math_sources[4];

static s32 skillset_math_collect_by_flags(s32 unit_id, u8 skillset, s16* out, u8 mask);
static s32 skillset_monster_has_supporter_at_tile(s32 team_flags, s32 x, s32 y, s32 total_height);

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
s32 skillset_get_unit_ability_data(
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
    s32 menu_flags;
    u8 requirements;
    u8 attributes;
    s32 turns;

    count = 0;
    materia_blade = 0;
    sword = 0;
    charge = 0;
    half_mp = 0;
    frog = 0;
    unit = unit_get_present_stats(unit_id);
    if (unit == 0) {
        return 0;
    }
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FROG)] & STATUS_BYTE_MASK(STATUS_ID_FROG)) { /* Frog */
        frog = 1;
    }
    if (unit->ability_flags.support[1] & UNIT_SUPPORT_SET_2_HALF_MP) {
        half_mp = 1;
    }
    if (unit->ability_flags.support[3] & UNIT_SUPPORT_SET_4_NON_CHARGE) {
        charge = 2;
    } else if (unit->ability_flags.support[3] & UNIT_SUPPORT_SET_4_SHORT_CHARGE) {
        charge = 1;
    }
    at_list_build(at_list, AT_LIST_MODE_ALL);
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
        known = ability_get_learned(unit, skillset_index);
    }
    /* The learned bits run MSB first from bit 23; only the 16 action slots are listed. */
    for (i = 0; i < 16; i++) {
        if (!(known & (0x800000 >> i))) {
            continue;
        }
        ability = ability_get_id_from_skillset(skillset, i);
        if (frog && ability != ABILITY_ID_BLACK_MAGIC_FROG) {
            continue;
        }
        if (ability == 0) {
            continue;
        }
        if (ability >= 1 && ability < ABILITY_ID_ITEM_FIRST) {
            secondary = &g_ability_range_data[ability];
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
            /* Pack Reflectable and Affected by Silence into the menu flag byte. */
            menu_flags = (attributes >> 7) | ((attributes & ABILITY_SECONDARY_FLAG_3_AFFECTED_BY_SILENCE) >> 4);
            if (!(attributes & ABILITY_SECONDARY_FLAG_3_PERSEVERE)) {
                if (charge == 1) {
                    ct = (ct + 1) >> 1;
                }
                if (charge == 2) {
                    ct = 0;
                }
            }
            turns = at_list_count_turns_to_resolve(unit_id, ct, at_list);
            mp_out[count] = mp;
            ct_out[count] = ct;
            flags_out[count] = menu_flags;
            turns_out[count] = turns;
        } else if (ability >= ABILITY_ID_CHARGE_FIRST && ability < ABILITY_ID_MATH_FIRST) {
            ct = (g_ability_charge_data - ABILITY_ID_CHARGE_FIRST)[ability].ct;
            turns = at_list_count_turns_to_resolve(unit_id, ct, at_list);
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

/* Load the unit's Item skillset entries through skillset_collect_item_abilities and return the
 * low byte of its result (the entry count). */
s32 skillset_load_item_abilities(s32 unit_id, s32 skillset, u8* out_item_ids) {
    return skillset_collect_item_abilities(unit_id, skillset & 0xFF, out_item_ids) & 0xFF;
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
s32 skillset_collect_item_abilities(s32 unit_id, u8 skillset, u8* out_item_ids) {
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
    unit = unit_get_present_stats(unit_id);
    if (unit == 0) {
        return 0;
    }
    if (skillset != SKILLSET_ID_ITEM) {
        *out_item_ids = ITEM_ID_NONE;
        return 0;
    }
    count = skillset_get_unit_ability_data(
        unit_id, SKILLSET_ID_ITEM, (s16*)abilities, mp_costs, ability_ct, 0, ability_flags, turns);
    if (count == 0) {
        *out_item_ids = ITEM_ID_NONE;
        return 0;
    }
    for (i = 0; i < count; i++) {
        if (abilities[i] >= ABILITY_ID_ITEM_FIRST && abilities[i] < ABILITY_ID_THROW_FIRST) {
            table = g_item_ability_item_ids;
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

/* The number of items skillset_collect_throwable_items lists in out_item_ids:
 * its result without the 0x100 marker. */
s32 skillset_load_throw_abilities(s32 unit_id, s32 skillset, u8* out_item_ids) {
    return skillset_collect_throwable_items(unit_id, skillset & 0xFF, out_item_ids) & 0xFF;
}

/* Collect the item ids the unit may throw into `out`, terminated with
 * ITEM_ID_NONE, and return 0x100 plus their count (0 when the unit or the
 * skillset is wrong or no Throw ability is learned).
 *
 * The skillset's abilities select item types through g_ability_throw_item_types; every item of
 * a selected type that is throwable and in stock is listed (item_decrement_party_quantity:
 * only a player-team unit can run out).
 */
s32 skillset_collect_throwable_items(s32 unit_id, u8 skillset, u8* out) {
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
    unit = unit_get_present_stats(unit_id);
    if (unit == 0) {
        return 0;
    }
    if (skillset != SKILLSET_ID_THROW
        || (ability_count = skillset_get_unit_ability_data(
                unit_id, SKILLSET_ID_THROW, (s16*)abilities, mp_costs, charge_times, 0, ability_flags, turn_counts))
            == 0) {
        *out = ITEM_ID_NONE;
        return 0;
    }
    for (i = 0; i < ability_count; i++) {
        if (abilities[i] >= ABILITY_ID_THROW_FIRST && abilities[i] < ABILITY_ID_JUMP_FIRST) {
            type_table = g_ability_throw_item_types;
            throw_index = abilities[i];
            throw_index = throw_index - ABILITY_ID_THROW_FIRST;
            item_types[i] = type_table[throw_index];
        }
    }
    for (i = 0; i < ITEM_ID_COUNT; i++) {
        item = &g_item_primary_data[i];
        type = item->type;
        found = 0;
        for (j = 0; j < ability_count; j++) {
            if (type == item_types[j]) {
                found = 1;
                break;
            }
        }
        if (found != 0 && (item->type_flags & ITEM_TYPE_FLAG_WEAPON) != 0
            && (g_item_weapon_data[item->secondary_data_id].flags & WEAPON_FLAG_THROWABLE) != 0) {
            item_id = i;
            if (item_decrement_party_quantity(unit, item_id, 0) == 0) {
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
s32 skillset_math_load_attributes(s32 unit_id, s32 skillset, s16* out_ability_ids) {
    return skillset_math_collect_by_flags(unit_id, skillset, out_ability_ids, CALCULATOR_ATTRIBUTE_MASK);
}

/* Collect the unit's learned Math Skill divisor abilities (Prime, 5, 4, 3)
 * into out_ability_ids. */
s32 skillset_math_load_multiples(s32 unit_id, s32 skillset, s16* out_ability_ids) {
    return skillset_math_collect_by_flags(unit_id, skillset, out_ability_ids, CALCULATOR_MULTIPLE_MASK);
}

/* Collect the unit's learned Math Skill abilities whose
 * g_ability_math_rsm_data flags intersect mask into out, -1-terminated,
 * and return the count. An empty list writes no terminator. */
static s32 skillset_math_collect_by_flags(s32 unit_id, u8 skillset, s16* out, u8 mask) {
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
    if (unit_get_present_stats(unit_id) == 0) {
        return 0;
    }
    if (skillset != SKILLSET_ID_MATH_SKILL) {
        *out = -1;
        return 0;
    }
    ability_count = skillset_get_unit_ability_data(
        unit_id, SKILLSET_ID_MATH_SKILL, (s16*)abilities, mp_costs, charge_times, 0, ability_flags, turn_counts);
    if (ability_count == 0) {
        return 0;
    }
    for (i = 0; i < ability_count; i++) {
        ability = abilities[i];
        if (ability >= ABILITY_ID_MATH_FIRST && ability < ABILITY_ID_REACTION_FIRST) {
            secondary_flags = &g_ability_math_rsm_data[ability - ABILITY_ID_MATH_FIRST];
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
 * in g_skillset_math_sources into out, -1-terminated, and return the
 * count. Only a request for the Math Skill skillset collects anything.
 *
 * The record pointer must be taken inside the inner loop: hoisting the table
 * base into a local before the loop swaps the two preheader address setups.
 */
s32 skillset_math_collect_calculator_abilities(s32 unit_id, u8 skillset, s16* out) {
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
    memcpy(skillsets, g_skillset_math_sources, sizeof(skillsets));
    if (unit_get_present_stats(unit_id) == 0) {
        return 0;
    }
    if (skillset != SKILLSET_ID_MATH_SKILL) {
        *out = -1;
        return 0;
    }
    for (i = 0; i < 4; i++) {
        ability_count = skillset_get_unit_ability_data(
            unit_id, skillsets[i], abilities, mp_costs, charge_times, 1, ability_flags, turn_counts);
        if (ability_count != 0) {
            for (j = 0; j < ability_count; j++) {
                ability = abilities[j];
                secondary = &g_ability_range_data[ability];
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
u16 skillset_get_elemental_ability(s32 unit_id, u8 skillset) {
    unit_stats_t* unit;
    s16 abilities[0x14];
    u8 mp_costs[0x18];
    u8 ability_ct[0x18];
    u8 ability_flags[0x18];
    u8 turns[0x18];
    s32 count;
    s32 i;
    s16 terrain_ability;

    unit = unit_get_present_stats(unit_id);
    if (unit == 0) {
        return 0;
    }
    if (skillset != SKILLSET_ID_ELEMENTAL) {
        return 0;
    }
    count = skillset_get_unit_ability_data(
        unit_id, SKILLSET_ID_ELEMENTAL, abilities, mp_costs, ability_ct, 1, ability_flags, turns);
    if (count == 0) {
        return 0;
    }
    terrain_ability
        = g_terrain_geomancy_abilities[g_battle_map_tile_data[battle_map_tile_calculate_unit_index(unit)].surface.type];
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
 * only the Frog spell. A completed list has no terminator and leaves the
 * result unset; early failures return 0. Its one caller ignores the result.
 * Keeping the s32 return preserves the original failure-path code.
 *
 * The target keeps two live copies of the ability id: `id` in $a0 feeds both
 * sign extensions and the `addiu -1` range check, while `ability` in $a1
 * carries the value the `sh` store writes and is shared with the Frog arm's
 * Frog Attack.
 */
s32 skillset_monster_collect_abilities(s32 unit_id, u8 skillset, s16* out, s32 unused, u8* flags_out) {
    unit_stats_t* unit;
    s32 count = 3;
    s32 frog = 0;
    s32 i;
    s16 id;
    s16 ability;
    u8 flags;

    unit = unit_get_present_stats(unit_id);
    if (unit == 0) {
        return 0;
    }
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FROG)] & STATUS_BYTE_MASK(STATUS_ID_FROG)) {
        frog = 1;
    }
    if (g_command_menu_types_by_skillset[skillset] != ACTION_MENU_TYPE_MONSTER) {
        *out = -1;
        return 0;
    }
    if (frog != 0) {
        ability = ABILITY_ID_FROG_ATTACK;
        *out = ability;
        out++;
    }
    if (skillset_monster_count_supporters(unit) != 0) {
        count = MONSTER_SKILLSET_ABILITY_COUNT;
    }
    for (i = 0; i < count; i++) {
        id = ability_get_id_from_skillset(skillset, i);
        ability = id;
        if ((frog == 0 || id == ABILITY_ID_BLACK_MAGIC_FROG) && id != 0 && id >= 1 && id < ABILITY_ID_ITEM_FIRST) {
            flags = g_ability_range_data[id].flags_3;
            /* Pack the same menu attributes as ordinary skillset abilities. */
            *flags_out++ = (flags >> 7) | ((flags & ABILITY_SECONDARY_FLAG_3_AFFECTED_BY_SILENCE) >> 4);
            *out = ability;
            out++;
        }
    }
}

/* Monster Skill check: the number of the four tiles adjacent to the unit that
 * hold a same-team Monster Skill supporter within 5 of the unit's total
 * height. Returns 0 for an empty slot. */
s32 skillset_monster_count_supporters(const unit_stats_t* unit) {
    unit_height_data_t height_data;
    s32 team;
    s32 x;
    s32 y;
    s32 height;
    s32 sum;

    if (unit->active_battle_id == UNIT_ACTIVE_BATTLE_ID_NONE) {
        return 0;
    }
    unit_calculate_height_data(&height_data, unit->battle_id);
    team = unit->team_flags;
    x = unit->x;
    y = unit->position.y;
    height = height_data.total_height;
    sum = skillset_monster_has_supporter_at_tile(team, x - 1, y, height)
        + skillset_monster_has_supporter_at_tile(team, x + 1, y, height);
    sum += skillset_monster_has_supporter_at_tile(team, x, y - 1, height);
    return sum + skillset_monster_has_supporter_at_tile(team, x, y + 1, height);
}

/* Return whether a unit on the team of `team_flags` that has the Monster Skill
 * support ability and can react stands on the tile, with a total height within
 * 5 of `total_height` (taken as a byte). */
static s32 skillset_monster_has_supporter_at_tile(s32 team_flags, s32 x, s32 y, s32 total_height) {
    unit_height_data_t height;
    u8 reference_height;
    s32 unit_index;
    s32 unit_team_flags = team_flags;

    if (x < 0 || x >= g_battle_map_terrain_width) {
        return 0;
    }
    if (y >= 0) {
        if (y >= g_battle_map_terrain_depth) {
            return 0;
        }

        unit_index = 0;
        reference_height = total_height;
        for (; unit_index < UNIT_SLOT_COUNT; unit_index++) {
            unit_stats_t* unit = &g_unit_stats[unit_index];

            if (unit->active_battle_id == UNIT_ACTIVE_BATTLE_ID_NONE || unit->x != x || unit->position.y != y
                || reaction_is_prevented(unit) != 0
                || !(unit->ability_flags.support[3] & UNIT_SUPPORT_SET_4_MONSTER_SKILL)
                || ((unit->team_flags ^ unit_team_flags) & UNIT_TEAM_MASK)) {
                continue;
            }
            unit_calculate_height_data(&height, unit->battle_id);
            if ((reference_height - height.total_height) >= -5 && (reference_height - height.total_height) <= 5) {
                return 1;
            }
        }
    }

    return 0;
}

/* The number of katanas skillset_collect_draw_out_katanas lists in
 * out_item_ids: its result without the 0x100 marker. */
s32 skillset_load_draw_out_abilities(s32 unit_id, s32 skillset, u8* out_item_ids) {
    return skillset_collect_draw_out_katanas(unit_id, skillset & 0xFF, out_item_ids) & 0xFF;
}

/* Collect the katana item id of each of the unit's learned Draw Out abilities
 * into out_item_ids, terminated by ITEM_ID_NONE, and return 0x100 plus their
 * count (0 when the unit or skillset is wrong or the list is empty). Draw Out
 * abilities run in the same order as their katanas. */
s32 skillset_collect_draw_out_katanas(s32 unit_id, u8 skillset, u8* out_item_ids) {
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
    if (unit_get_present_stats(unit_id) == 0) {
        return 0;
    }
    if (skillset != SKILLSET_ID_DRAW_OUT) {
        *out_item_ids = ITEM_ID_NONE;
        return 0;
    }
    ability_count = skillset_get_unit_ability_data(
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

/* skillset_load_charge_abilities for a caller that only wants the ability ids;
 * the charge times and turn counts go to scratch arrays. */
s32 skillset_load_charge_ability_ids(s32 unit_id, u8 skillset_id, s16* ability_ids) {
    u8 charge_times[16];
    u8 turn_counts[16];

    return skillset_load_charge_abilities(unit_id, skillset_id, ability_ids, charge_times, turn_counts);
}

/* Load the skillset's usable abilities with their CT and turn counts into the
 * caller's lists; the MP-cost and silence/reflect flag lists are scratch. */
s32 skillset_load_charge_abilities(s32 unit_id, s32 skillset, s16* abilities, u8* ability_ct, u8* turns) {
    u8 mp_costs[16];
    u8 ability_flags[16];

    return skillset_get_unit_ability_data(unit_id, skillset, abilities, mp_costs, ability_ct, 0, ability_flags, turns);
}
