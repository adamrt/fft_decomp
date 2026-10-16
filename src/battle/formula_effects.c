/* Formula helpers for brave and speed changes, elemental and weather modifiers, faith, HP and MP absorption, undead
 * reversal, status procs, equipment breaks, poach, catch, and hit and evade calculations. */

#include "fft/ability.h"
#include "fft/action.h"
#include "fft/formula.h"
#include "fft/item.h"
#include "fft/job.h"
#include "fft/memory.h"
#include "fft/move.h"
#include "fft/reaction.h"
#include "fft/status.h"
#include "fft/unit.h"
#include "fft/unit_runtime.h"
#include "fft/weather.h"
#include "psx/types.h"

/* Effect of each talk skill: Negotiate takes gil, Persuade zeroes CT, Praise
 * and Threaten change Brave by Y, Preach and Solution change Faith by Y; the
 * rest inflict their status. */
void formula_apply_talk_skill(void) {
    switch (g_action_current_ability.ability_id) {
    case ABILITY_ID_TALK_NEGOTIATE:
        formula_calculate_stolen_gil();
        return;
    case ABILITY_ID_TALK_PERSUADE:
        g_action_target_data->ct_change = ACTION_CT_CHANGE_ZERO;
        break;
    case ABILITY_ID_TALK_PRAISE:
        g_action_target_data->brave_change = g_action_current_ability.range_data.y | ACTION_STAT_CHANGE_INCREASE;
        break;
    case ABILITY_ID_TALK_THREATEN:
        g_action_target_data->brave_change = g_action_current_ability.range_data.y & ACTION_STAT_CHANGE_VALUE_MASK;
        break;
    case ABILITY_ID_TALK_PREACH:
        g_action_target_data->faith_change = g_action_current_ability.range_data.y | ACTION_STAT_CHANGE_INCREASE;
        break;
    case ABILITY_ID_TALK_SOLUTION:
        g_action_target_data->faith_change = g_action_current_ability.range_data.y & ACTION_STAT_CHANGE_VALUE_MASK;
        break;
    default:
        formula_apply_status_to_action();
        return;
    }
    g_action_target_data->attack_type = ACTION_TYPE_PSEUDO_STATUS;
}

/* Formula 0x3A: +Brave (Y). */
void formula_add_brave_y(void) {
    g_action_target_data->brave_change = g_action_current_ability.range_data.y | ACTION_STAT_CHANGE_INCREASE;
    g_action_target_data->attack_type = ACTION_TYPE_PSEUDO_STATUS;
}

/* Formula 0x39: +Speed (Y). */
void formula_add_speed_y(void) {
    g_action_target_data->sp_change = g_action_current_ability.range_data.y | ACTION_STAT_CHANGE_INCREASE;
    g_action_target_data->attack_type = ACTION_TYPE_PSEUDO_STATUS;
}

/* Formula 0x3B: +Brave (X), +PA/MA/Speed (Y). */
void formula_add_brave_x_stats_y(void) {
    s32 y;

    g_action_target_data->brave_change = g_action_current_ability.range_data.x | ACTION_STAT_CHANGE_INCREASE;
    y = g_action_current_ability.range_data.y | ACTION_STAT_CHANGE_INCREASE;
    g_action_target_data->pa_change = y;
    g_action_target_data->ma_change = y;
    g_action_target_data->sp_change = y;
    g_action_target_data->attack_type = ACTION_TYPE_PSEUDO_STATUS;
}

/* Formula 0x3C: damage the caster for a fifth of its max HP and heal the
 * target for twice the damage stored (read back as a 16-bit value). */
void formula_3c_damage_caster_max_hp_one_fifth_heal_target_two_fifths(void) {
    u16 amount = g_action_attacker->max_hp / 5;
    s32 dealt;
    action_data_t* action = g_action_attacker_data;
    action_data_t* target;

    action->attack_type = ACTION_TYPE_HP_DAMAGE;
    action->hp_damage = amount;
    action = g_action_attacker_data; /* reloaded, as retail does */
    action->hit = 1;
    target = g_action_target_data;
    dealt = g_action_attacker_data->hp_damage;
    target->attack_type = ACTION_TYPE_HP_HEALING;
    target->hp_healing = dealt * 2;
}

/* Formula 0x43: damage equal to the caster's missing HP (max HP - HP). */
void formula_43_damage_caster_missing_hp(void) {
    unit_stats_t* unit = g_action_attacker;
    action_data_t* action = g_action_target_data;
    u16 max_hp = unit->max_hp;
    u16 hp = unit->hp;

    action->attack_type = ACTION_TYPE_HP_DAMAGE;
    action->hp_damage = max_hp - hp;
}

/* Formula 0x44: HP damage equal to the target's current MP. */
void formula_damage_hp_target_current_mp(void) {
    u16 mp = g_action_target->mp;
    action_data_t* action = g_action_target_data;

    action->attack_type = ACTION_TYPE_HP_DAMAGE;
    action->hp_damage = mp;
}

/* Formula 0x45: damage equal to the target's missing HP (max HP - HP). */
void formula_damage_target_missing_hp(void) {
    unit_stats_t* unit = g_action_target;
    action_data_t* action = g_action_target_data;

    action->hp_damage = unit->max_hp - unit->hp;
    action->attack_type = ACTION_TYPE_HP_DAMAGE;
}

/* Brave loss of Y: the amount is stored without
 * ACTION_STAT_CHANGE_INCREASE. */
void formula_set_brave_loss_y(void) {
    g_action_target_data->brave_change = g_action_current_ability.range_data.y & ACTION_STAT_CHANGE_VALUE_MASK;
    g_action_target_data->attack_type = ACTION_TYPE_PSEUDO_STATUS;
}

/* Fire loses and lightning gains a quarter of its HP damage in a storm or a
 * strong storm; ice gains a quarter in a snowstorm or a strong snowstorm.
 * Plain rain and snow have no effect. The weather comes through
 * weather_get_effective, so an indoor map has none, unlike the bow
 * penalty of formula_evade_apply_weather_on_bows. */
void formula_element_apply_weather(void) {
    u8 element;
    s32 weather;

    element = g_action_current_ability.range_data.element;
    weather = weather_get_effective();
    if (weather == WEATHER_STORM || weather == WEATHER_STRONG_STORM) {
        if (element & ELEMENT_FIRE) {
            g_action_target_data->hp_damage = g_action_target_data->hp_damage * 3 / 4;
        }
        if (element & ELEMENT_LIGHTNING) {
            g_action_target_data->hp_damage = g_action_target_data->hp_damage * 5 / 4;
        }
    }
    if (weather == WEATHER_SNOWSTORM || weather == WEATHER_STRONG_SNOWSTORM) {
        if (element & ELEMENT_ICE) {
            g_action_target_data->hp_damage = g_action_target_data->hp_damage * 5 / 4;
        }
    }
}

/* Target affinities to the weapon's elements. */
void formula_element_modify_weapon_damage(void) {
    formula_element_modify_damage(g_action_current_ability.weapon_data.element);
}

/* Element step, run after the HP damage is stored: Oil against fire, Float
 * against earth (a rider uses its mount's Float), then the target's affinities
 * to the ability's elements. */
void formula_element_apply(void) {
    u8 element;
    action_data_t* action;
    unit_stats_t* unit;
    s32 mount_info;

    element = g_action_current_ability.range_data.element;
    /* Fire removes Oil. XA is doubled, but every caller has already stored the
     * HP damage and nothing reads XA afterwards, so the damage does not change. */
    if ((g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_OIL)] & STATUS_BYTE_MASK(STATUS_ID_OIL))
        && (element & ELEMENT_FIRE)) {
        action = g_action_target_data;
        g_action_current_ability.xa = g_action_current_ability.xa * 2;
        action->status_changes.removal[STATUS_BYTE_INDEX(STATUS_ID_OIL)] |= STATUS_BYTE_MASK(STATUS_ID_OIL);
        if (status_modify_inflictions(0) != 0) {
            g_action_target_data->attack_type = ACTION_TYPE_STATUS_CHANGE;
        }
    }
    unit = g_action_target;
    mount_info = unit->mount_info;
    if (mount_info & UNIT_MOUNT_INFO_FLAG_RIDER) {
        unit = &g_unit_stats[mount_info & UNIT_MOUNT_INFO_PARTNER_ID_MASK];
    }
    /* Float nullifies earth. */
    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FLOAT)] & STATUS_BYTE_MASK(STATUS_ID_FLOAT))
        && (element & ELEMENT_EARTH)) {
        formula_nullify_action();
        g_action_target_data->miss_type = ACTION_MISS_TYPE_FORCED_FAILURE;
    }
    formula_element_modify_damage(element);
}

/* Turn absorbed elemental HP damage into HP healing; otherwise mark the action as HP damage. */
void formula_element_apply_absorption(void) {
    action_data_t* action = g_action_target_data;

    if (action->miss_type != ACTION_MISS_TYPE_NULLIFIED) {
        if (action->special_effect & ACTION_SPECIAL_EFFECT_ELEMENTAL_ABSORPTION) {
            u16 damage = action->hp_damage;
            action->hp_damage = 0;
            action->attack_type = ACTION_TYPE_HP_HEALING;
            action->hp_healing = damage;
        } else {
            action->attack_type = ACTION_TYPE_HP_DAMAGE;
        }
    }
}

/* Faith step: HP damage * caster Faith * target Faith / 10000. The Faith
 * status counts as 100 and Innocent as 0; Innocent wins when a unit has both.
 * The two Faith values come from attacker_faith and target_faith, which the
 * action setup loaded. */
void formula_calculate_faith(void) {
    action_data_t* action;

    if (g_action_attacker->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FAITH)]
        & STATUS_BYTE_MASK(STATUS_ID_FAITH)) {
        g_action_current_ability.attacker_faith = 100;
    }
    if (g_action_attacker->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_INNOCENT)]
        & STATUS_BYTE_MASK(STATUS_ID_INNOCENT)) {
        g_action_current_ability.attacker_faith = 0;
    }
    if (g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FAITH)] & STATUS_BYTE_MASK(STATUS_ID_FAITH)) {
        g_action_current_ability.target_faith = 100;
    }
    if (g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_INNOCENT)]
        & STATUS_BYTE_MASK(STATUS_ID_INNOCENT)) {
        g_action_current_ability.target_faith = 0;
    }

    action = g_action_target_data;
    action->hp_damage
        = action->hp_damage * g_action_current_ability.target_faith * g_action_current_ability.attacker_faith / 10000;
}

/* Drain step: once the target's result is final, a hit heals the attacker by
 * the HP damage. Against an undead target it is reversed: the attacker takes
 * the damage and the target is healed by it. The several pointer locals are
 * the retail code's separate loads of the same globals. */
void formula_apply_hp_absorption(void) {
    action_data_t* action;
    action_data_t* attacker;
    action_data_t* action2;
    action_data_t* action3;
    action_data_t* attacker2;
    u16 amount;

    if (g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_UNDEAD)]
        & STATUS_BYTE_MASK(STATUS_ID_UNDEAD)) {
        action_finalize_target_result();
        action = g_action_target_data;
        if (action->hit != 0) {
            attacker = g_action_attacker_data;
            attacker->hp_damage = action->hp_damage;
            attacker->attack_type = ACTION_TYPE_HP_DAMAGE;
            g_action_attacker_data->hit = 1;
            action2 = g_action_target_data;
            amount = action2->hp_damage;
            action2->hp_damage = 0;
            action2->attack_type = ACTION_TYPE_HP_HEALING;
            action2->hp_healing = amount;
        }
        return;
    }
    action_finalize_target_result();
    action3 = g_action_target_data;
    if (action3->hit != 0) {
        attacker2 = g_action_attacker_data;
        attacker2->hp_healing = action3->hp_damage;
        attacker2->attack_type = ACTION_TYPE_HP_HEALING;
        g_action_attacker_data->hit = 1;
        g_action_target_data->attack_type = ACTION_TYPE_HP_DAMAGE;
    }
}

/* Healing step: the stored HP damage becomes healing. An undead target takes
 * it as damage instead, and the ability no longer counts as canceling the
 * statuses of the first status byte (Dead among them). */
void formula_apply_undead_reversal(void) {
    action_data_t* action;
    action_data_t* action_again;
    u16 hp_damage;

    if (g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_UNDEAD)]
        & STATUS_BYTE_MASK(STATUS_ID_UNDEAD)) {
        g_action_target_data->attack_type = ACTION_TYPE_HP_DAMAGE;
        g_action_canceled_statuses[0] = 0;
        return;
    }
    action = g_action_target_data;
    action->attack_type = action->attack_type & ~ACTION_TYPE_HP_DAMAGE;
    hp_damage = action->hp_damage;
    action_again = g_action_target_data;
    action->hp_damage = 0;
    action->hp_healing = hp_damage;
    action_again->attack_type = action_again->attack_type | ACTION_TYPE_HP_HEALING;
}

/* Result of formula 0x0E: an undead target is healed by the amount and
 * receives none of the statuses of the first status byte (Dead among them); a
 * target immune to Dead is missed; anyone else takes it as HP damage. */
void formula_apply_undead_absorb_attack(void) {
    unit_stats_t* unit = g_action_target;

    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_UNDEAD)] & STATUS_BYTE_MASK(STATUS_ID_UNDEAD)) {
        u16 amount;
        action_data_t* action = g_action_target_data;
        action_data_t* next;

        action->attack_type = ACTION_TYPE_HP_HEALING;
        amount = action->hp_damage;
        next = g_action_target_data;
        action->hp_damage = 0;
        action->hp_healing = amount;
        next->status_changes.infliction[0] = 0;
    } else if (unit->status_sets.immunity[STATUS_BYTE_INDEX(STATUS_ID_DEAD)] & STATUS_BYTE_MASK(STATUS_ID_DEAD)) {
        formula_force_attack_miss();
    } else {
        g_action_target_data->attack_type = ACTION_TYPE_HP_DAMAGE;
    }
}

/* MP drain step: the stored HP damage becomes MP damage on the target; when it
 * lands, the attacker recovers the same amount of MP. */
void formula_apply_mp_absorption(void) {
    u16 mp_amount;
    action_data_t* action = g_action_target_data;
    action_data_t* action_after;

    mp_amount = action->hp_damage;
    action->hp_damage = 0;
    action->attack_type = ACTION_TYPE_MP_DAMAGE;
    action->mp_damage = mp_amount;
    action_finalize_target_result();
    action_after = g_action_target_data;
    if (action_after->hit != 0) {
        action_data_t* reaction = g_action_attacker_data;
        reaction->mp_healing = action_after->mp_damage;
        reaction->attack_type = ACTION_TYPE_MP_HEALING;
        g_action_attacker_data->hit = 1;
    }
}

/* Turn the stored HP damage into MP healing on the target (formula 0x54 heals MA * Y MP this way). */
void formula_convert_hp_damage_to_mp_recovery(void) {
    action_data_t* action = g_action_target_data;
    u16 hp_damage;

    hp_damage = action->hp_damage;
    action->hp_damage = 0;
    action->attack_type = ACTION_TYPE_MP_HEALING;
    action->mp_healing = hp_damage;
}

/* Use the percentage in hp_damage as the hit chance and clear it. 100 or more
 * always hits. Below that the shown accuracy is scaled by it and, when
 * executing, a roll of 0..99 at or above it misses; a chance of 0 misses in
 * every state. */
void formula_use_hp_damage_as_action_hit_percent(void) {
    s32 percent;
    s32 scaled;
    s32 state;
    s32 accuracy;
    action_data_t* action = g_action_target_data;

    percent = action->hp_damage;
    if (percent >= 100) {
        action->hp_damage = 0;
        return;
    }
    accuracy = action->attack_accuracy;
    state = g_action_state;
    scaled = percent * accuracy / 100;
    action->hp_damage = 0;
    action->attack_accuracy = scaled;
    if (state != ACTION_STATE_EXECUTE && percent != 0) {
        return;
    }
    if (random_roll_pass_fail(100, percent) != 0) {
        formula_cause_action_miss();
        g_action_target_data->hp_damage = 0;
    }
}

/* Miss unless the target's monster graphic is 0x0F or 0x10 (one unsigned
 * compare tests both): the dragon and hydra jobs and the Holy Dragon. */
void formula_check_dragon(void) {
    if ((g_action_target->monster_graphic - 0xF) >= 2U) {
        formula_force_attack_miss();
    }
}

/* Force a miss when the target is asleep. Talk skills, songs and dances run this first. */
void formula_force_sleeping_target_miss(void) {
    if ((g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_SLEEP)] & STATUS_BYTE_MASK(STATUS_ID_SLEEP))
        != 0) {
        formula_force_attack_miss();
    }
}

/* Maintenance on the target turns a break or steal into a forced failure. */
void formula_apply_maintenance(void) {
    if (g_action_target->ability_flags.support[2] & UNIT_SUPPORT_SET_3_MAINTENANCE) {
        g_action_target_data->hit = 0;
        g_action_target_data->miss_type = ACTION_MISS_TYPE_FORCED_FAILURE;
        g_action_target_data->attack_accuracy = 0;
    }
}

/* When executing, a roll of 0..99 below FORMULA_ADDED_EFFECT_CHANCE (19)
 * sets ACTION_SPECIAL_EFFECT_ADDED_EFFECT. */
static void formula_roll_conditional_status_proc_inner(void) {
    if (g_action_state == ACTION_STATE_EXECUTE && random_roll_pass_fail(100, FORMULA_ADDED_EFFECT_CHANCE) == 0) {
        action_data_t* action = g_action_target_data;
        /* The 16-bit read-modify-write sets the added-effect flag. */
        action->special_effect = action->special_effect | ACTION_SPECIAL_EFFECT_ADDED_EFFECT;
    }
}

/* Queue the weapon's proc ability (proc_id) to strike the current target after
 * the action, in place of a status, and cancel any pending knockback. */
void formula_queue_weapon_spell(void) {
    g_action_current_ability.weapon_proc_pending = 1;
    g_action_current_ability.reaction_id = g_action_current_ability.proc_id;
    g_action_current_ability.post_action_target_id = g_action_current_ability.target_id;
    move_knockback_cancel();
}

/* Magic gun: select the Fire line for a fire gun, else Bolt for lightning, else
 * Ice (also for any other element or none), and the tier from a roll of 0..99:
 * below 10 the third spell, below 40 the second, otherwise the first
 * (10/30/60%). The spell becomes the current ability, in the attacker's action
 * command too, and its ability data is installed. */
void formula_select_magic_gun_ability(void) {
    u8 element;
    u8 pct;
    u16 id;
    s32 roll;

    element = g_action_current_ability.weapon_data.element;
    roll = formula_get_random();
    pct = (roll * 100) / 0x8000;
    if (element & ELEMENT_FIRE) {
        if (pct < 10) {
            id = ABILITY_ID_BLACK_MAGIC_FIRE_3;
        } else {
            id = ABILITY_ID_BLACK_MAGIC_FIRE;
            if (pct < 40) {
                id = ABILITY_ID_BLACK_MAGIC_FIRE_2;
            }
        }
    } else if (element & ELEMENT_LIGHTNING) {
        if (pct < 10) {
            id = ABILITY_ID_BLACK_MAGIC_BOLT_3;
        } else {
            id = ABILITY_ID_BLACK_MAGIC_BOLT;
            if (pct < 40) {
                id = ABILITY_ID_BLACK_MAGIC_BOLT_2;
            }
        }
    } else {
        if (pct < 10) {
            id = ABILITY_ID_BLACK_MAGIC_ICE_3;
        } else {
            id = ABILITY_ID_BLACK_MAGIC_ICE;
            if (pct < 40) {
                id = ABILITY_ID_BLACK_MAGIC_ICE_2;
            }
        }
    }
    g_action_current_ability.ability_id = id;
    g_action_attacker->action_command.ability_id = id;
    mem_copy_byte_data_source_first(
        &g_ability_range_data[id], &g_action_current_ability.range_data, sizeof(g_action_current_ability.range_data));
}

/* Quick: fill the target's CT (ACTION_CT_CHANGE_QUICK) as a pseudo-status change. */
void formula_apply_quick_effect(void) {
    g_action_target_data->ct_change = ACTION_CT_CHANGE_QUICK;
    g_action_target_data->attack_type = ACTION_TYPE_PSEUDO_STATUS;
}

/* Lower PA (Power Break, Power Ruin), MA (Mind Break, Mind Ruin) or Speed
 * (Speed Break, Speed Ruin) by X; other abilities change nothing. */
void formula_determine_reduced_stat(void) {
    u16 ability = g_action_current_ability.ability_id;

    if (ability == ABILITY_ID_BATTLE_SKILL_POWER_BREAK || ability == ABILITY_ID_DESTROY_SWORD_POWER_RUIN) {
        g_action_target_data->pa_change = g_action_current_ability.range_data.x & ACTION_STAT_CHANGE_VALUE_MASK;
    } else if (ability == ABILITY_ID_BATTLE_SKILL_MIND_BREAK || ability == ABILITY_ID_DESTROY_SWORD_MIND_RUIN) {
        g_action_target_data->ma_change = g_action_current_ability.range_data.x & ACTION_STAT_CHANGE_VALUE_MASK;
    } else if (ability == ABILITY_ID_BATTLE_SKILL_SPEED_BREAK || ability == ABILITY_ID_DESTROY_SWORD_SPEED_RUIN) {
        g_action_target_data->sp_change = g_action_current_ability.range_data.x & ACTION_STAT_CHANGE_VALUE_MASK;
    } else {
        return;
    }
    g_action_target_data->attack_type |= ACTION_TYPE_PSEUDO_STATUS;
}

/* Draw Out: the used katana breaks when a roll of 0..99 is below its weapon
 * power. The roll is made only when executing; any other state records it as
 * broken. A Mime's katana is not touched. */
void formula_calculate_katana_break_chance(void) {
    if (g_action_attacker->job_id == JOB_ID_MIME) {
        return;
    }

    g_action_attacker_data->hit = 1;
    g_action_attacker_data->item_lost = g_action_attacker->action_command.item_or_equipment;
    g_action_attacker_data->miss_type = ACTION_MISS_TYPE_CANCELED;

    if (g_action_state == ACTION_STATE_EXECUTE
        && random_roll_pass_fail(100, g_action_current_ability.weapon_data.power) != 0) {
        g_action_attacker_data->special_effect = ACTION_SPECIAL_EFFECT_DRAW_OUT_KATANA_NOT_BROKEN;
    } else {
        g_action_attacker_data->special_effect = ACTION_SPECIAL_EFFECT_DRAW_OUT_KATANA_BROKEN;
    }
}

/* Pick the equipment slot a Steal, Break or Mighty Sword ability targets.
 *
 * Helmet, armor, shield (the right hand's, else the left's), accessory and
 * weapon (the hand whose item has the higher required level, right on ties)
 * abilities record the slot bit in equipment_destroyed and the item in
 * item_lost, returning 0. Monsters, empty slots and other abilities clear
 * equipment_destroyed and return -1. */
s32 formula_select_targeted_equipment(void) {
    u16 ability;
    s32 right_level;
    s32 left_level;

    g_action_target_data->equipment_destroyed = ACTION_EQUIPMENT_SLOT_ALL;
    if (!(g_action_target->unit_flags & UNIT_FLAG_MONSTER)) {
        ability = g_action_current_ability.ability_id;
        if (ability == ABILITY_ID_BATTLE_SKILL_HEAD_BREAK || ability == ABILITY_ID_MIGHTY_SWORD_BLASTAR_PUNCH
            || ability == ABILITY_ID_STEAL_HELMET) {
            if (g_action_target->equipment[UNIT_EQUIPMENT_SLOT_HEAD] != ITEM_ID_NONE) {
                g_action_target_data->equipment_destroyed = ACTION_EQUIPMENT_SLOT_HEAD;
                g_action_target_data->item_lost = g_action_target->equipment[UNIT_EQUIPMENT_SLOT_HEAD];
                return 0;
            }
        } else if (ability == ABILITY_ID_BATTLE_SKILL_ARMOR_BREAK || ability == ABILITY_ID_MIGHTY_SWORD_SHELLBUST_STAB
            || ability == ABILITY_ID_STEAL_ARMOR) {
            if (g_action_target->equipment[UNIT_EQUIPMENT_SLOT_BODY] != ITEM_ID_NONE) {
                g_action_target_data->equipment_destroyed = ACTION_EQUIPMENT_SLOT_BODY;
                g_action_target_data->item_lost = g_action_target->equipment[UNIT_EQUIPMENT_SLOT_BODY];
                return 0;
            }
        } else if (ability == ABILITY_ID_BATTLE_SKILL_SHIELD_BREAK || ability == ABILITY_ID_STEAL_SHIELD) {
            if (g_action_target->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD] != ITEM_ID_NONE) {
                g_action_target_data->equipment_destroyed = ACTION_EQUIPMENT_SLOT_RIGHT_SHIELD;
                g_action_target_data->item_lost = g_action_target->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD];
                return 0;
            }
            if (g_action_target->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD] != ITEM_ID_NONE) {
                g_action_target_data->equipment_destroyed = ACTION_EQUIPMENT_SLOT_LEFT_SHIELD;
                g_action_target_data->item_lost = g_action_target->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD];
                return 0;
            }
        } else if (ability == ABILITY_ID_MIGHTY_SWORD_ICEWOLF_BITE || ability == ABILITY_ID_STEAL_ACCESSORY) {
            if (g_action_target->equipment[UNIT_EQUIPMENT_SLOT_ACCESSORY] != ITEM_ID_NONE) {
                g_action_target_data->equipment_destroyed = ACTION_EQUIPMENT_SLOT_ACCESSORY;
                g_action_target_data->item_lost = g_action_target->equipment[UNIT_EQUIPMENT_SLOT_ACCESSORY];
                return 0;
            }
        } else if (ability == ABILITY_ID_BATTLE_SKILL_WEAPON_BREAK || ability == ABILITY_ID_MIGHTY_SWORD_HELLCRY_PUNCH
            || ability == ABILITY_ID_STEAL_WEAPON) {
            left_level = -1;
            right_level = -1;
            if (g_action_target->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] != ITEM_ID_NONE) {
                right_level = g_item_primary_data[g_action_target->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON]]
                                  .required_level;
            }
            if (g_action_target->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] != ITEM_ID_NONE) {
                left_level = g_item_primary_data[g_action_target->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON]]
                                 .required_level;
            }
            if (right_level != -1 || left_level != right_level) {
                if (right_level >= left_level) {
                    g_action_target_data->equipment_destroyed = ACTION_EQUIPMENT_SLOT_RIGHT_WEAPON;
                    g_action_target_data->item_lost = g_action_target->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON];
                    return 0;
                }
                g_action_target_data->equipment_destroyed = ACTION_EQUIPMENT_SLOT_LEFT_WEAPON;
                g_action_target_data->item_lost = g_action_target->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON];
                return 0;
            }
        }
    }
    g_action_target_data->equipment_destroyed = 0;
    return -1;
}

/* Resolve Secret Hunt and Train after an HP-damaging formula 01-06 hit. Its
 * only caller skips every later formula, so Throw (0x63) and Jump (0x64) never
 * poach or train.
 *
 * Only monsters (neither sex bit set) qualify, and not through a team Golem
 * while the ability can be blocked by it, nor when dead, petrified or
 * walled. A killing blow from a Secret Hunt attacker poaches a monster job
 * target; a hit leaving the target at or below 1/5 max HP lets a Train
 * attacker inflict Invite. */
void formula_apply_poach_and_train(void) {
    action_data_t* action = g_action_target_data;
    unit_stats_t* target;
    s32 remaining;

    if (!(action->attack_type & ACTION_TYPE_HP_DAMAGE)) {
        return;
    }
    target = g_action_target;
    remaining = target->hp - action->hp_damage + action->hp_healing;
    if (target->unit_flags & (UNIT_FLAG_FEMALE | UNIT_FLAG_MALE)) {
        return;
    }
    if ((g_action_team_golem[(target->initial_team_flags & UNIT_TEAM_MASK) >> UNIT_TEAM_SHIFT] != 0)
        && (g_action_current_ability.range_data.flags_3 & ABILITY_SECONDARY_FLAG_3_BLOCKED_BY_GOLEM)) {
        return;
    }
    if ((target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEAD)] & STATUS_BYTE_MASK(STATUS_ID_DEAD))
        || (target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_PETRIFY)] & STATUS_BYTE_MASK(STATUS_ID_PETRIFY))) {
        return;
    }
    if (target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_WALL)] & STATUS_BYTE_MASK(STATUS_ID_WALL)) {
        return;
    }
    if (((remaining <= 0)
            || (action->status_changes.infliction[STATUS_BYTE_INDEX(STATUS_ID_DEAD)]
                & STATUS_BYTE_MASK(STATUS_ID_DEAD)))
        && (g_action_attacker->ability_flags.support[2] & UNIT_SUPPORT_SET_3_SECRET_HUNT)
        && !(target->team_flags & UNIT_TEAM_FLAG_IMMORTAL)) {
        if (target->job_id < JOB_ID_MONSTER_FIRST || target->job_id > JOB_ID_MONSTER_LAST) {
            return;
        }
        move_knockback_cancel();
        formula_clear_pending_proc();
        g_action_target_data->special_effect |= ACTION_SPECIAL_EFFECT_POACH;
        g_action_current_ability.post_action_message = ACTION_POST_MESSAGE_POACHED;
        return;
    }
    if ((remaining > 0) && ((g_action_target->max_hp / 5) >= remaining)
        && (g_action_attacker->ability_flags.support[2] & UNIT_SUPPORT_SET_3_TRAIN)) {
        g_action_target_data->status_changes.infliction[STATUS_BYTE_INDEX(STATUS_ID_INVITE)]
            |= STATUS_BYTE_MASK(STATUS_ID_INVITE);
        if (status_modify_inflictions(0) != 0) {
            g_action_target_data->special_effect |= ACTION_SPECIAL_EFFECT_TEAM_CHANGE;
            g_action_target_data->attack_type |= ACTION_TYPE_STATUS_CHANGE;
            g_action_current_ability.post_action_message = ACTION_POST_MESSAGE_TRAINED;
        }
    }
}

/* Add the ability's status as a side effect of a hit: the hit flag, accuracy
 * and miss type are kept when the status does not apply. */
void formula_apply_status(void) {
    u8 hit, miss_type;
    u16 accuracy;

    miss_type = g_action_target_data->miss_type;
    /* The accuracy field is read and written as a 16-bit value (lhu/sh). */
    accuracy = g_action_target_data->attack_accuracy;
    hit = g_action_target_data->hit;
    formula_apply_status_to_action();
    g_action_target_data->hit = hit;
    g_action_target_data->attack_accuracy = accuracy;
    g_action_target_data->miss_type = miss_type;
}

/* Apply the current ability's status set to the target action. Also the
 * handler of formula 0x38 (status, 100%).
 *
 * All-or-nothing copies the full set, random chooses one status, separate
 * rolls each status independently, and cancel writes the removal set. Separate
 * keeps a status when a roll of 0..99 is below 24, while the shown accuracy is
 * divided by four. Preview and AI evaluation force probabilistic modes through
 * the all-or-nothing path. A set that changes nothing on the target is a
 * forced miss. */
void formula_apply_status_to_action(void) {
    s32 status_ids[STATUS_COUNT];
    s32 type;
    s32 count;
    s32 status;
    s32 selected_status;
    s32 byte;
    s32 mask;
    u8 any;
    action_data_t* action;

    type = g_action_current_ability.status_infliction.type;
    if (type & STATUS_INFLICTION_TYPE_SEPARATE) {
        g_action_target_data->attack_accuracy /= 4;
    }
    if (type & (STATUS_INFLICTION_TYPE_SEPARATE | STATUS_INFLICTION_TYPE_RANDOM_ONE)) {
        if (g_action_state != ACTION_STATE_EXECUTE) {
            type = STATUS_INFLICTION_TYPE_ALL_OR_NOTHING;
        }
    }

    if (type & STATUS_INFLICTION_TYPE_ALL_OR_NOTHING) {
        status = 0;
        do {
            g_action_target_data->status_changes.infliction[status]
                |= g_action_current_ability.status_infliction.statuses[status];
            status++;
        } while (status < STATUS_BYTE_COUNT);
    } else if (type & STATUS_INFLICTION_TYPE_RANDOM_ONE) {
        count = 0;
        status = 0;
        do {
            if (g_action_current_ability.status_infliction.statuses[status / 8] & (0x80 >> (status & 7))) {
                status_ids[count] = status;
                count++;
            }
            status++;
        } while (status < STATUS_COUNT);

        selected_status = status_ids[(formula_get_random() * count) / 32768];
        g_action_target_data->status_changes.infliction[selected_status / 8] |= 0x80 >> (selected_status & 7);
    } else if (type & STATUS_INFLICTION_TYPE_SEPARATE) {
        status = 0;
        do {
            byte = status / 8;
            if (g_action_current_ability.status_infliction.statuses[byte] & (mask = 0x80 >> (status & 7))) {
                /* Drop the status from the set unless a 24 in 100 roll keeps
                 * it; the preview test cannot pass, as only EXECUTE gets here. */
                if (random_roll_pass_fail(100, 24) != 0 || g_action_state == ACTION_STATE_PREVIEW) {
                    g_action_current_ability.status_infliction.statuses[byte] ^= mask;
                }
            }
            status++;
        } while (status < STATUS_COUNT);

        status = 0;
        do {
            g_action_target_data->status_changes.infliction[status]
                |= g_action_current_ability.status_infliction.statuses[status];
            status++;
        } while (status < STATUS_BYTE_COUNT);
    } else if (type & STATUS_INFLICTION_TYPE_CANCEL) {
        status = 0;
        do {
            g_action_target_data->status_changes.removal[status]
                |= g_action_current_ability.status_infliction.statuses[status];
            status++;
        } while (status < STATUS_BYTE_COUNT);
    }

    any = 0;
    status = 0;
    action = g_action_target_data;
    do {
        any |= action->status_changes.infliction[status] | action->status_changes.removal[status];
        status++;
    } while (status < STATUS_BYTE_COUNT);

    if (any == 0 || status_modify_inflictions(0) == 0) {
        formula_force_attack_miss();
        return;
    }

    g_action_target_data->attack_type |= ACTION_TYPE_STATUS_CHANGE;
    if (g_action_target_data->status_changes.infliction[STATUS_BYTE_INDEX(STATUS_ID_INVITE)]
        & STATUS_BYTE_MASK(STATUS_ID_INVITE)) {
        g_action_target_data->special_effect |= ACTION_SPECIAL_EFFECT_TEAM_CHANGE;
    }
}

/* Formula 0x49: heal (Z * 10) MP, Z from the used item's data. */
void formula_heal_mp_z_times_ten(void) {
    s32 index = g_action_current_ability.used_item_id;
    s32 amount;
    action_data_t* action = g_action_target_data;

    amount = g_item_secondary_data[index].z;
    action->attack_type = ACTION_TYPE_MP_HEALING;
    action->mp_healing = amount * 10;
}

/* Store max HP as HP damage and max MP as MP healing. Both callers then run
 * formula_apply_undead_reversal, which turns the HP part into healing
 * (damage for an undead target). */
void formula_apply_full_hp_mp_heal(void) {
    unit_stats_t* unit = g_action_target;
    action_data_t* action = g_action_target_data;

    action->hp_damage = unit->max_hp;
    action->mp_healing = unit->max_mp;
    action->attack_type = ACTION_TYPE_HP_DAMAGE | ACTION_TYPE_MP_HEALING;
}

/* Cancel the action with Finger Guard when the target's Brave-based reaction
 * roll succeeds. The shown accuracy becomes 100 - Brave, and a Brave of 100
 * always guards, even outside execution. */
void reaction_try_finger_guard(void) {
    unit_stats_t* unit;
    s32 chance;

    if (reaction_is_blocked(g_action_target) != 0)
        return;
    unit = g_action_target;
    if ((unit->ability_flags.reaction[3] & UNIT_REACTION_SET_4_FINGER_GUARD) == 0)
        return;
    chance = 100 - unit->brave;
    /* The target stores the accuracy as a 16-bit value (0x2a..0x2b). */
    g_action_target_data->attack_accuracy = chance;
    if (chance != 0) {
        if (reaction_fails_brave_roll(unit) != 0)
            return;
        if (g_action_state != ACTION_STATE_EXECUTE)
            return;
    }
    g_action_target_data->hit = 0;
    g_action_target_data->reaction_id = ABILITY_ID_REACTION_FINGER_GUARD;
    g_action_target_data->miss_type = ACTION_MISS_TYPE_BLADE_GRASP_OR_FINGER_GUARD;
}

/* Catch the thrown weapon when the target's Brave-based reaction roll succeeds,
 * as reaction_try_finger_guard decides it; the caught weapon goes in
 * last_received_attack. A Mime attacker's throw is never caught. */
void reaction_try_catch(void) {
    unit_stats_t* unit;
    action_data_t* action;
    /* Loaded through a pointer so GCC keeps it between the struct stores
     * (a plain scalar load is assumed not to alias them and floats). */
    u8* weapon_id_ptr = &g_action_current_ability.weapon_id;
    s32 weapon_id;
    s32 chance;

    if (reaction_is_blocked(g_action_target) != 0)
        return;
    if (g_action_attacker->job_id == JOB_ID_MIME)
        return;
    unit = g_action_target;
    if ((unit->ability_flags.reaction[3] & UNIT_REACTION_SET_4_CATCH) == 0)
        return;
    chance = 100 - unit->brave;
    /* The target stores the accuracy as a 16-bit value (0x2a..0x2b). */
    g_action_target_data->attack_accuracy = chance;
    if (chance != 0) {
        if (reaction_fails_brave_roll(unit) != 0)
            return;
        if (g_action_state != ACTION_STATE_EXECUTE)
            return;
    }
    g_action_target_data->hit = 0;
    action = g_action_target_data;
    action->reaction_id = ABILITY_ID_REACTION_CATCH;
    weapon_id = *weapon_id_ptr;
    action->miss_type = ACTION_MISS_TYPE_CATCH;
    action->last_received_attack = weapon_id;
}

/* Roll the evades (formula_evade_calculate_final_hit_percent). Returns 1 when
 * the attack was evaded. */
static s32 formula_evade_roll(void) {
    formula_evade_calculate_final_hit_percent();
    return g_action_target_data->hit == 0;
}

/* Added-effect roll of the damage formulas: returns 0 when the status or proc
 * applies. When executing that is a roll of 0..99 below
 * FORMULA_ADDED_EFFECT_CHANCE (19); a preview always shows it; the AI
 * simulation never counts it. */
s32 formula_roll_conditional_status_proc(void) {
    formula_roll_conditional_status_proc_inner();
    if (g_action_state == ACTION_STATE_PREVIEW) {
        return 0;
    }
    if (g_action_target_data->special_effect & ACTION_SPECIAL_EFFECT_ADDED_EFFECT) {
        return 0;
    }
    return 1;
}

/* Physical evade step: load the target's evades, then Concentrate, a Dark,
 * Confused or Transparent attacker, Abandon, the target's statuses, the bow
 * weather penalty and facing; then roll. Returns 1 when the attack was evaded. */
s32 formula_evade_calculate_physical(void) {
    formula_evade_store_physical();
    formula_evade_calculate_concentrate();
    formula_evade_calculate_dark_confuse();
    formula_evade_calculate_abandon();
    formula_evade_apply_target_statuses();
    formula_evade_apply_weather_on_bows();
    formula_evade_calculate_facing();
    return formula_evade_roll();
}

/* formula_evade_calculate_physical without the bow weather penalty. */
s32 formula_evade_calculate_physical_without_weather(void) {
    formula_evade_store_physical();
    formula_evade_calculate_concentrate();
    formula_evade_calculate_dark_confuse();
    formula_evade_calculate_abandon();
    formula_evade_apply_target_statuses();
    formula_evade_calculate_facing();
    return formula_evade_roll();
}

/* Magic evade step: load the target's magic evades, then a Transparent
 * attacker, Abandon and the target's statuses; then roll. Returns 1 when the
 * attack was evaded. */
s32 formula_evade_calculate_magical(void) {
    formula_evade_store_magical();
    formula_evade_calculate_transparent();
    formula_evade_calculate_abandon();
    formula_evade_apply_target_statuses();
    return formula_evade_roll();
}

/* Physical modifiers on XA, in order: attacker supports (Two Hands, Attack Up,
 * Martial Arts), Berserk or Frog, Defense Up, the target's statuses, zodiac
 * compatibility. */
void formula_apply_physical_modifiers(void) {
    formula_apply_physical_attack_supports();
    formula_apply_attacker_berserk_frog();
    formula_apply_defense_up();
    formula_apply_target_physical_statuses();
    formula_apply_zodiac_compatibility();
}

/* Damage of formulas 0x01 and 0x02: XA and YA by weapon type, charge power,
 * weapon element strengthen, physical modifiers, critical roll, XA * YA, then
 * the target's affinity to the weapon's elements. Returns 0 when the added
 * effect applies (formula_roll_conditional_status_proc), 1 when it does
 * not or the element canceled the attack. */
s32 formula_calculate_weapon_damage(void) {
    formula_calculate_base_xa();
    formula_apply_charge();
    formula_element_apply_weapon_strengthen();
    formula_calculate_physical_damage();
    formula_element_modify_weapon_damage();
    if (g_action_target_data->hit == 0)
        return 1;
    formula_element_apply_absorption();
    return formula_roll_conditional_status_proc();
}

/* Physical damage: XA * YA after the physical modifiers and the critical roll. */
void formula_calculate_physical_damage(void) {
    formula_apply_physical_modifiers();
    formula_calculate_critical_hit();
    formula_store_xa_times_ya_damage();
}

/* Hit chance (XA+YA)% with every physical modifier on XA, Two Hands included.
 * Returns 1 on a miss. */
s32 formula_accuracy_calculate_physical(void) {
    formula_apply_physical_modifiers();
    formula_store_hit_chance();
    formula_use_hp_damage_as_action_hit_percent();
    return g_action_target_data->hit == 0;
}

/* Absorption, then the added-effect roll. Returns the roll's result: 0 when
 * the caller should apply the status. */
s32 formula_element_apply_absorption_and_status_proc(void) {
    formula_element_apply_absorption();
    return formula_roll_conditional_status_proc();
}

/* Magical modifiers on XA, in order: Magic Attack Up, Magic Defense Up, the
 * target's statuses (Shell, Frog or Chicken), zodiac compatibility. */
void formula_apply_magical_modifiers(void) {
    formula_apply_magic_attack_up();
    formula_apply_magic_defense_up();
    formula_apply_target_magical_statuses();
    formula_apply_zodiac_compatibility();
}

/* Store XA * YA as HP damage, then the weather and element steps. Returns 1
 * when the element canceled the action. */
s32 formula_calculate_elemental_xa_times_ya(void) {
    formula_store_xa_times_ya_damage();
    formula_element_apply_weather();
    formula_element_apply();
    return g_action_target_data->hit == 0;
}

/* Absorption, then the added-effect roll; when it succeeds the ability's
 * status is applied. Returns the roll's result (0 when applied). */
s32 formula_element_apply_absorption_and_status(void) {
    s32 result;

    formula_element_apply_absorption();
    result = formula_roll_conditional_status_proc();
    if (result == 0) {
        formula_apply_status();
        return 0;
    }
    return result;
}
