/* Build each unit's usable AI ability list and derive its weapon and targeting properties. */

#include "fft/ability.h"
#include "fft/ai.h"
#include "fft/ai_decision.h"
#include "fft/battle_map.h"
#include "fft/command.h"
#include "fft/item.h"
#include "fft/job.h"
#include "fft/skillset.h"
#include "fft/status.h"
#include "fft/unit.h"
#include "fft/unit_runtime.h"
#include "psx/types.h"

static s32 ability_usable_from_skillset_add(s32 unit_id, s32 skillset, s32 count);

static void usable_abilities_build(s32 unit_id);

static s32 ratio_q2_calculate(s32 divisor, s32 dividend);

static void ability_weapon_attack_data_store(ai_weapon_data_t* out, s32 item_id, s32* flags_out);

/*
 * Initialize AI workspace defaults and present-unit indexing.
 *
 * Average maximum HP includes all present slots and is capped at 255.
 */
void ai_workspace_init(void) {
    s32 i = 0;
    s32 present_count = 0;
    s32 total_max_hp = 0;
    ai_workspace_t* ai = &g_ai_workspace_storage;

    g_ai_workspace = ai;
    g_ai_unit_snapshots = g_ai_unit_snapshot_storage;
    ai->resume_pending = 0;
    ai->selected_action.rank.decided = 0;
    ai->selected_action.coords.fields.zero = 0;
    ai->hardcoded_status_abilities[0].skillset = SKILLSET_ID_BLOOD_SUCK_STATUS;
    ai->hardcoded_status_abilities[0].packed_id = ABILITY_ID_BLOOD_SUCK;
    ai->hardcoded_status_abilities[0].usage.flags = AI_ABILITY_ENTRY_USABLE | AI_ABILITY_ENTRY_EXHAUSTIVE_ORIGIN_SCAN;
    ai->hardcoded_status_abilities[1].skillset = SKILLSET_ID_FROG_ATTACK;
    ai->hardcoded_status_abilities[1].packed_id = ABILITY_ID_FROG_ATTACK;
    ai->hardcoded_status_abilities[1].usage.flags = AI_ABILITY_ENTRY_USABLE | AI_ABILITY_ENTRY_EXHAUSTIVE_ORIGIN_SCAN;
    ai->present_unit_average_max_hp = 0;
    ai->terrain_width = g_battle_map_terrain_width;
    ai->terrain_depth = g_battle_map_terrain_depth;
    for (i = 0; i < UNIT_SLOT_COUNT; i++) {
        ai->unit_compact_indices[i] = 0xff;
        if (g_unit_stats[i].active_battle_id != UNIT_ACTIVE_BATTLE_ID_NONE) {
            ai->unit_compact_indices[i] = present_count;
            ai_unit_abilities_init(i);
            present_count++;
            total_max_hp += g_unit_stats[i].max_hp;
        }
    }
    if (present_count != 0) {
        s32 average = total_max_hp / present_count;
        if (average >= 256) {
            ai->present_unit_average_max_hp = 255;
        } else {
            ai->present_unit_average_max_hp = average;
        }
    }
    {
        s32 empty = 0xff;
        for (i = AI_KNOWN_TRAP_COUNT - 1; i >= 0; i--) {
            ai->known_trap_coords[i].fields.x = empty;
        }
    }
    ai->throw_ability_id = ABILITY_ID_THROW_KNIFE;
    ai->throw_weapon_id = ITEM_ID_DAGGER;
}

/*
 * Initialize a unit's ability-use statistics and scenario targeting.
 *
 * Monster Skill-dependent entries count toward totals but not the MP,
 * Silence, or evasion numerators. With the special-behavior flag, Conserve CT
 * selects the RETREAT auto-battle mode; a focus target is turned from a unit
 * id into a battle slot and, with Aggressive, also becomes the auto-battle
 * target under TARGETED (another initial team) or RECOVERY (the same team).
 * A focus target not on the field clears the special behavior.
 */
void ai_unit_abilities_init(s32 unit_id) {
    ai_workspace_t* ai = &g_ai_workspace_storage;
    unit_stats_t* unit = &g_unit_stats[unit_id];
    ai_unit_decision_t* decision;
    ai_ability_entry_t* entry;
    s32 ability_count;
    s32 mp_count;
    s32 silence_count;
    s32 evade_count;
    s32 i;

    if (unit->active_battle_id == UNIT_ACTIVE_BATTLE_ID_NONE
        || ai->unit_compact_indices[unit_id] >= AI_COMPACT_UNIT_COUNT) {
        return;
    }
    usable_abilities_build(unit_id);
    ability_count = 0;
    mp_count = 0;
    silence_count = 0;
    evade_count = 0;
    decision = &ai->unit_decisions[unit_id];
    decision->highest_mp_cost = 0;
    decision->lowest_mp_cost = 0xff;
    for (i = 0; i < AI_ABILITY_LIST_COUNT; i++) {
        entry = &ai->ability_lists[ai->unit_compact_indices[unit_id]][i];
        if (entry->skillset == SKILLSET_ID_NONE) {
            break;
        }
        ai_ability_entry_load(entry);
        ability_count++;
        /* The target reads and updates skillset/usage as one 16-bit value. */
        if (!entry->usage.policy.requires_monster_skill) {
            if (ai->considered_ability.ai_flags.word & AI_ABILITY_AFFECTED_BY_SILENCE) {
                silence_count++;
            }
            if (ai->considered_ability.ai_flags.word & AI_ABILITY_EVADEABLE) {
                evade_count++;
            }
            if (ai->considered_ability.mp_cost != 0) {
                mp_count++;
                if (decision->highest_mp_cost < ai->considered_ability.mp_cost) {
                    decision->highest_mp_cost = ai->considered_ability.mp_cost;
                }
                if (ai->considered_ability.mp_cost < decision->lowest_mp_cost) {
                    decision->lowest_mp_cost = ai->considered_ability.mp_cost;
                }
            }
        }
        entry->usage.flags &= AI_ABILITY_ENTRY_INIT_PRESERVE_MASK;
        if (ai->considered_ability.ai_flags.word & (AI_ABILITY_DIRECT_TRAJECTORY | AI_ABILITY_ARC_TRAJECTORY)) {
            continue;
        }
        if (ai->considered_ability.ai_flags.fields.flags_3 & AI_ABILITY_FLAG_3_EXHAUSTIVE_ORIGIN_MASK) {
            entry->usage.policy.exhaustive_origin_scan = 1;
        } else {
            if (ai->considered_ability.ability_id < ABILITY_ID_ITEM_FIRST) {
                if (ai->considered_ability.parameters.fields.range + ai->considered_ability.parameters.fields.aoe < 3
                    && (g_ability_range_data[ai->considered_ability.ability_id].flags_4
                        & ABILITY_SECONDARY_FLAG_4_EVADEABLE)) {
                    entry->usage.policy.exhaustive_origin_scan = 1;
                    continue;
                }
                if (g_ability_range_data[ai->considered_ability.ability_id].flags_2
                    & (ABILITY_SECONDARY_FLAG_2_CANNOT_HIT_CASTER | ABILITY_SECONDARY_FLAG_2_CAN_TARGET_ENEMIES)) {
                    entry->usage.policy.force_consume_evaluated_aim_tiles = 1;
                    continue;
                }
            }
            if (ai->considered_ability.parameters.fields.aoe == 0) {
                entry->usage.policy.force_consume_evaluated_aim_tiles = 1;
            }
        }
    }
    decision->silence_mod = ratio_q2_calculate(ability_count, silence_count);
    decision->mp_ability_mod = ratio_q2_calculate(ability_count, mp_count);
    decision->evade_mod = ratio_q2_calculate(ability_count, evade_count);
    /* The five ENTD AI bytes become flags, main target, both targeting flag bytes and
     * the enemy flag; the stay-near tile's level is the top bit of the first. */
    ai_mem_copy_bytes(&decision->flags, &unit->ai.fields.flags, 5);
    decision->target.fields.x = unit->ai.fields.target_x;
    decision->target.fields.y = unit->ai.fields.target_y;
    decision->target.fields.layer = unit->ai.fields.flags >> 7;
    decision->target.fields.zero = 0;
    if (!(decision->flags & AI_DECISION_SPECIAL_BEHAVIOR)) {
        return;
    }
    if (decision->targeting_flags_2 & AI_TARGET_CONSERVE_CT) {
        ai->unit_autobattle_modes[unit_id] = AI_AUTOBATTLE_RETREAT;
        return;
    }
    if (!(decision->flags & AI_DECISION_FOCUS_TARGET)) {
        return;
    }
    for (i = 0; i < UNIT_SLOT_COUNT; i++) {
        if (g_unit_stats[i].active_battle_id == UNIT_ACTIVE_BATTLE_ID_NONE
            || g_unit_stats[i].unit_id != decision->main_target_id) {
            continue;
        }
        decision->main_target_id = i;
        if (!(decision->flags & AI_DECISION_AGGRESSIVE)) {
            return;
        }
        unit->auto_battle_target = i;
        if ((g_unit_stats[i].initial_team_flags & UNIT_TEAM_MASK) != (unit->initial_team_flags & UNIT_TEAM_MASK)) {
            ai->unit_autobattle_modes[unit_id] = AI_AUTOBATTLE_FIGHT_FOR_LIFE;
        } else {
            ai->unit_autobattle_modes[unit_id] = AI_AUTOBATTLE_PROTECT_ALLIES;
        }
        return;
    }
    decision->flags &= ~AI_DECISION_SPECIAL_BEHAVIOR;
}

/* dividend * 4 / divisor, or 0 for a zero divisor: a share in quarters, such
 * as how many of a unit's abilities Silence stops. */
static s32 ratio_q2_calculate(s32 divisor, s32 dividend) {
    if (divisor == 0) {
        return 0;
    }
    return dividend * 4 / divisor;
}

/* Byte-oriented view of the same four-byte ability entry. */
typedef struct ability_entry_byte_view {
    u8 ability_id_low;
    u8 unit_and_id_high;
    u8 skillset;
    u8 usage_flags;
} ability_entry_byte_view_t;
typedef char ability_entry_byte_view_size_must_be_4[sizeof(ability_entry_byte_view_t) == 4 ? 1 : -1];

/*
 * Build a unit's ability list and weapon-attack properties.
 *
 * Non-monster skillsets include Attack and a secondary skillset. Frog Attack
 * is appended for every unit; a terminator is written only if space remains.
 */
static void usable_abilities_build(s32 unit_id) {
    unit_stats_t* unit = &g_unit_stats[unit_id];
    ai_workspace_t* ai = &g_ai_workspace_storage;
    ai_weapon_data_t* weapon;
    s32 list_id = ai->unit_compact_indices[unit_id];
    s32 count = 0;
    s32 weapon_flags;

    if (unit->primary_skillset < SKILLSET_ID_MONSTER_FIRST) {
        ai->ability_lists[list_id][0].skillset = SKILLSET_ID_ATTACK;
        ai->ability_lists[list_id][0].packed_id = unit_id << AI_ABILITY_ENTRY_UNIT_SHIFT;
        weapon = &ai->unit_weapon_data[list_id];
        weapon->words[0] = 0;
        weapon->words[1] = 0;
        weapon->fields.flags_1 |= AI_ABILITY_FLAG_1_AFFECTS_HP;
        weapon->fields.flags_2 |= AI_ABILITY_FLAG_2_EVADEABLE;
        weapon->fields.flags_3 |= AI_ABILITY_FLAG_3_PHYSICAL_ATTACK;
        weapon_flags = AI_WEAPON_FORMULA_NONE;
        if (!(unit->unit_flags & UNIT_FLAG_MONSTER)) {
            ability_weapon_attack_data_store(
                weapon, unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON], &weapon_flags);
            ability_weapon_attack_data_store(
                weapon, unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON], &weapon_flags);
        }
        switch (weapon_flags) {
        case AI_WEAPON_FORMULA_NONE:
            weapon->fields.range = 1;
            /* Fall through to normal enemy targeting. */
        case AI_WEAPON_FORMULA_DAMAGE:
            weapon->fields.flags_1 |= AI_ABILITY_FLAG_1_TARGET_ENEMIES;
            break;
        case AI_WEAPON_FORMULA_ABSORB_HP:
            weapon->fields.flags_1 |= AI_ABILITY_FLAG_1_TARGET_ENEMIES;
            weapon->fields.flags_2 |= AI_ABILITY_FLAG_2_UNDEAD_REVERSE;
            break;
        case AI_WEAPON_FORMULA_HEAL:
            weapon->fields.flags_1 |= AI_ABILITY_FLAG_1_TARGET_ALLIES;
            weapon->fields.flags_2 |= AI_ABILITY_FLAG_2_UNDEAD_REVERSE;
            break;
        }
        count++;
    }
    count = ability_usable_from_skillset_add(unit_id, unit->primary_skillset, count);
    if (unit->primary_skillset < SKILLSET_ID_MONSTER_FIRST) {
        count = ability_usable_from_skillset_add(unit_id, unit->secondary_skillset, count);
    }
    ai->ability_lists[list_id][count].skillset = SKILLSET_ID_FROG_ATTACK;
    ai->ability_lists[list_id][count].packed_id = (unit_id << AI_ABILITY_ENTRY_UNIT_SHIFT) + ABILITY_ID_FROG_ATTACK;
    /* The byte view preserves the target's separate final-store address. */
    ((ability_entry_byte_view_t*)&ai->ability_lists[list_id][count])->usage_flags
        = AI_ABILITY_ENTRY_USABLE | AI_ABILITY_ENTRY_EXHAUSTIVE_ORIGIN_SCAN;
    count++;
    if (count < AI_ABILITY_LIST_COUNT) {
        ai->ability_lists[list_id][count].skillset = SKILLSET_ID_NONE;
    }
}

/*
 * Merge one weapon's attack properties into the unit's AI weapon summary.
 *
 * Range and weapon ID are replaced; elements and trajectory/formula flags
 * accumulate. Calling for the left hand last gives its range precedence.
 */
static void ability_weapon_attack_data_store(ai_weapon_data_t* out, s32 item_id, s32* flags_out) {
    weapon_data_t* weapon;
    s32 flags;
    s32 formula;

    if (item_id == ITEM_ID_NONE) {
        return;
    }
    weapon = &g_item_weapon_data[item_id];
    flags = weapon->flags;
    if (flags & WEAPON_FLAG_ARC) {
        out->fields.flags_3 |= AI_ABILITY_FLAG_3_ARC_TRAJECTORY;
    } else if (flags & WEAPON_FLAG_DIRECT) {
        out->fields.flags_3 |= AI_ABILITY_FLAG_3_DIRECT_TRAJECTORY;
    }
    formula = weapon->formula;
    switch (formula) {
    case ABILITY_FORMULA_WEAPON_ABSORB_HP:
        *flags_out |= AI_WEAPON_FORMULA_ABSORB_HP;
        break;
    case ABILITY_FORMULA_WEAPON_HEAL:
        *flags_out |= AI_WEAPON_FORMULA_HEAL;
        break;
    default:
        *flags_out |= AI_WEAPON_FORMULA_DAMAGE;
        break;
    }
    out->fields.weapon_id = item_id;
    out->fields.range = weapon->range;
    out->fields.element |= weapon->element;
}

/* Append a skillset's usable abilities to the unit's AI ability list and
 * return the updated count.
 *
 * Throw adds three shuriken and three ball entries, then at most one entry for
 * the first other known throw ability; Math Skill adds a single entry;
 * Charge is skipped with a gun equipped. Other skillsets add each AI-usable,
 * known ability whose MP cost fits the unit's maximum MP and whose
 * sword/materia-blade requirement passes; a unit with innate Innocent adds
 * none while the workspace's considered ability, not the one being added, is
 * Faith-based. Jump keeps only the best level-jump ability; without one it
 * writes a bare Jump entry that the returned count does not include. For
 * monster skillsets only the fourth ability keeps the monster-skill flag.
 *
 * The known-throw tail sits before the main body because the target emits it
 * there and GCC 2.6.3 keeps source block order. The per-loop temporaries,
 * ai as a local and the int max_mp reproduce the target's allocation, loop
 * set-up order and signed comparison. */
static s32 ability_usable_from_skillset_add(s32 unit_id, s32 skillset, s32 count) {
    s32 jump_found;
    ai_workspace_t* ai = &g_ai_workspace_storage;
    u16* abilities;
    s32 menu;
    s32 list_id;
    s32 i;
    ai_ability_entry_t* entry;
    ai_ability_entry_t* slot;
    ai_ability_entry_t* row;
    unit_stats_t* unit;
    s32 ability_id;
    s32 unit_bits;
    u8 mp_cost;
    s32 max_mp;
    u8 flags;

    if (skillset == 0) {
        return count;
    }
    jump_found = 0;
    abilities = ability_skillset_list_build(skillset, SKILLSET_ABILITY_FILTER_ACTION);
    menu = g_command_menu_types_by_skillset[skillset];
    list_id = ai->unit_compact_indices[unit_id];
    if (menu == ACTION_MENU_TYPE_WEAPON_INVENTORY) {
        if (ai_ability_is_known(unit_id, skillset, 0)) {
            unit_bits = unit_id << AI_ABILITY_ENTRY_UNIT_SHIFT;
            for (i = 0, slot = ai->ability_lists[list_id]; i < 3; i++) {
                slot[count].skillset = skillset;
                slot[count].packed_id = unit_bits + i + ITEM_ID_THROWABLE_FIRST;
                count++;
            }
        }
        if (ai_ability_is_known(unit_id, skillset, 1)) {
            i = 0;
            unit_bits = unit_id << AI_ABILITY_ENTRY_UNIT_SHIFT;
            for (row = ai->ability_lists[list_id]; i < 3; i++) {
                row[count].skillset = skillset;
                row[count].packed_id = unit_bits + i + ITEM_ID_FIRE_BALL;
                count++;
            }
        }
        for (i = 2; i < 12; i++) {
            if (ai_ability_is_known(unit_id, skillset, i)) {
                entry = &ai->ability_lists[list_id][count];
                count++;
                entry->skillset = skillset;
                entry->packed_id = unit_id << AI_ABILITY_ENTRY_UNIT_SHIFT;
                break;
            }
        }
        return count;
    }
    if (menu == ACTION_MENU_TYPE_MATH_SKILL) {
        entry = &ai->ability_lists[list_id][count];
        entry->skillset = skillset;
        entry->packed_id = unit_id << AI_ABILITY_ENTRY_UNIT_SHIFT;
        return count + 1;
    }
    if (menu == ACTION_MENU_TYPE_CHARGE) {
        i = ai->unit_weapon_data[list_id].fields.weapon_id;
        if (i >= ITEM_ID_ROMANDA_GUN && i <= ITEM_ID_BLAST_GUN) {
            return count;
        }
    }
    unit = &g_unit_stats[unit_id];
    for (i = 0; i < HUMAN_SKILLSET_ACTION_SLOT_COUNT; i++) {
        ability_id = abilities[i];
        if (ability_id == 0) {
            continue;
        }
        entry = &ai->ability_lists[list_id][count];
        if (!(g_ability_data[ability_id].ai_flags.flags_4 & AI_ABILITY_FLAG_4_USABLE_BY_AI)) {
            continue;
        }
        if (ability_id < ABILITY_ID_ITEM_FIRST) {
            mp_cost = g_ability_range_data[ability_id].mp_cost;
            if (unit->ability_flags.support[1] & UNIT_SUPPORT_SET_2_HALF_MP) {
                mp_cost >>= 1;
            }
            max_mp = unit->max_mp;
            if (max_mp < mp_cost) {
                continue;
            }
            if ((ai->considered_ability.ai_flags.word & AI_ABILITY_AFFECTED_BY_FAITH)
                && (unit->status_sets.innate[STATUS_BYTE_INDEX(STATUS_ID_INNOCENT)]
                    & STATUS_BYTE_MASK(STATUS_ID_INNOCENT))) {
                continue;
            }
            flags = g_ability_range_data[ability_id].flags_4;
            if (flags & ABILITY_SECONDARY_FLAG_4_REQUIRES_SWORD) {
                if (!(unit->equipped_flags & UNIT_EQUIPPED_FLAG_SWORD)) {
                    continue;
                }
            } else if (flags & ABILITY_SECONDARY_FLAG_4_REQUIRES_MATERIA_BLADE) {
                if (!(unit->equipped_flags & UNIT_EQUIPPED_FLAG_MATERIA_BLADE)) {
                    continue;
                }
            }
        }
        if (!ai_ability_is_known(unit_id, skillset, i)) {
            continue;
        }
        if (skillset >= SKILLSET_ID_MONSTER_FIRST) {
            if (i == 3) {
                entry->usage.policy.requires_monster_skill = 1;
            } else {
                entry->usage.policy.requires_monster_skill = 0;
            }
        }
        entry->skillset = skillset;
        entry->packed_id = ability_id;
        if (menu == ACTION_MENU_TYPE_JUMP) {
            if (ability_id >= ABILITY_ID_JUMP_VERTICAL_2) {
                continue;
            }
            if (jump_found == 1) {
                count--;
                entry = &ai->ability_lists[list_id][count];
                entry->packed_id = ability_id;
            }
            jump_found = 1;
        }
        count++;
        entry->packed_id += unit_id << AI_ABILITY_ENTRY_UNIT_SHIFT;
    }
    if (menu == ACTION_MENU_TYPE_JUMP && jump_found == 0) {
        entry->skillset = SKILLSET_ID_JUMP;
        entry->packed_id = unit_id << AI_ABILITY_ENTRY_UNIT_SHIFT;
    }
    return count;
}

/* Whether the unit has learned the given ability bit of a skillset: returns 1
 * for monsters (every ability known) and for skillsets the unit does not own.
 * Job skillsets 5..23 map to learned_abilities rows 0..18; the unit's own
 * primary or base-job skillset maps to row 0. */
s32 ai_ability_is_known(s32 unit_id, s32 skillset_id, s32 bit) {
    unit_stats_t* unit = &g_unit_stats[unit_id];
    s32 byte;
    u8* row;

    if ((unit->unit_flags & UNIT_FLAG_MONSTER) != 0) {
        return 1;
    }
    if (skillset_id >= SKILLSET_ID_BASIC_SKILL && skillset_id < SKILLSET_ID_MIMIC) {
        skillset_id = skillset_id - SKILLSET_ID_BASIC_SKILL;
    } else {
        if (skillset_id != unit->primary_skillset && skillset_id != unit->base_job_skillset)
            return 1;
        skillset_id = 0;
    }
    /* Typed indexing moves the field offset ahead of the unit-base addition.
     * Keep the target's address order, deriving the offset from the field. */
    row = (u8*)(skillset_id * UNIT_LEARNED_ABILITY_BYTES_PER_JOB + (u32)unit
        + ((u32)unit->career.learned_abilities - (u32)unit));
    byte = bit / 8;
    return (row[byte] & (0x80 >> (bit - (byte * 8)))) != 0;
}
