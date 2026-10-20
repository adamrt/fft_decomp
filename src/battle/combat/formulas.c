/* Implement the ability-formula catalog: weapon and magic attacks, recovery, stat and status changes, equipment skills
 * and transformations. */

#include "fft/ability.h"
#include "fft/action.h"
#include "fft/formula.h"
#include "fft/item.h"
#include "fft/job.h"
#include "fft/reaction.h"
#include "fft/status.h"
#include "fft/unit.h"
#include "fft/unit_identity.h"
#include "psx/types.h"

/* Hit chance F(XA+YA)% for magic on allies: Magic Attack Up and zodiac
 * compatibility, none of the target's defenses. Returns 1 on a miss. */
static s32 friendly_magic_accuracy_calculate(void) {
    formula_apply_magic_attack_up();
    formula_apply_zodiac_compatibility();
    formula_store_xa_plus_ya();
    formula_calculate_faith();
    formula_use_hp_damage_as_action_hit_percent();
    return g_action_target_data->hit == 0;
}

/* XA * YA with Magic Attack Up and zodiac compatibility only: the healing
 * formulas ignore the target's defenses. */
void formula_calculate_magical_xa_times_ya(void) {
    formula_apply_magic_attack_up();
    formula_apply_zodiac_compatibility();
    formula_store_xa_times_ya_damage();
}

/* Golem's hit chance: (MA with Magic Attack Up + X) * caster Faith / 100, the
 * Faith status counting as 100 and Innocent as 0. No zodiac compatibility and
 * no target Faith. Returns 1 on a miss. */
static s32 golem_accuracy_calculate(void) {
    action_data_t* action;

    formula_store_ma_and_x();
    formula_apply_magic_attack_up();
    formula_store_xa_plus_ya();
    if ((g_action_attacker->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FAITH)] & STATUS_BYTE_MASK(STATUS_ID_FAITH))
        != 0) {
        g_action_work.attacker_faith = 100;
    }
    if ((g_action_attacker->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_INNOCENT)]
            & STATUS_BYTE_MASK(STATUS_ID_INNOCENT))
        != 0) {
        g_action_work.attacker_faith = 0;
    }
    action = g_action_target_data;
    /* The target reads hp_damage signed (lh). */
    action->hp_damage = action->hp_damage * g_action_work.attacker_faith / 100;
    formula_use_hp_damage_as_action_hit_percent();
    return g_action_target_data->hit == 0;
}

/* Magical damage without Faith: elemental strengthen, magical modifiers, XA *
 * YA with weather and element, absorption, 19% status. */
void formula_calculate_magical_damage_without_faith(void) {
    formula_element_apply_strengthen();
    formula_apply_magical_modifiers();
    if (formula_calculate_elemental_xa_times_ya() == 0) {
        formula_element_apply_absorption_and_status();
    }
}

/* Damage MA * (MA + Y) / 2 without Faith: stores XA and YA, then runs the magical
 * damage chain of formula_calculate_magical_damage_without_faith. */
void formula_calculate_truth_damage(void) {
    formula_store_ma_and_ma_plus_y_divided_by_two();
    formula_calculate_magical_damage_without_faith();
}

/* formula_accuracy_calculate_magic without the elemental strengthen. */
static s32 magic_accuracy_without_strengthen_calculate(void) {
    formula_store_ma_and_x();
    formula_apply_magical_modifiers();
    formula_store_xa_plus_ya();
    formula_calculate_faith();
    formula_use_hp_damage_as_action_hit_percent();
    return g_action_target_data->hit == 0;
}

/* Hit chance F(MA+X)%: elemental strengthen and the magical modifiers on MA,
 * zodiac compatibility on both terms, then Faith. Returns 1 on a miss. */
s32 formula_accuracy_calculate_magic(void) {
    formula_store_ma_and_x();
    formula_element_apply_strengthen();
    formula_apply_magical_modifiers();
    formula_store_xa_plus_ya();
    formula_calculate_faith();
    formula_use_hp_damage_as_action_hit_percent();
    return g_action_target_data->hit == 0;
}

/* formula_accuracy_calculate_magic without the Faith step: hit (MA+X)%. */
s32 formula_accuracy_calculate_magic_without_faith(void) {
    formula_store_ma_and_x();
    formula_element_apply_strengthen();
    formula_apply_magical_modifiers();
    formula_store_xa_plus_ya();
    formula_use_hp_damage_as_action_hit_percent();
    return g_action_target_data->hit == 0;
}

/* The physical modifiers on XA without the attacker supports: Berserk or Frog,
 * Defense Up, the target's statuses, zodiac compatibility. */
void formula_apply_physical_modifiers_without_supports(void) {
    formula_apply_attacker_berserk_frog();
    formula_apply_defense_up();
    formula_apply_target_physical_statuses();
    formula_apply_zodiac_compatibility();
}

/* Hit chance (XA+YA)% with Attack Up, Martial Arts, Berserk or Frog, Defense
 * Up, the target's statuses and zodiac compatibility on XA. Returns 1 on a
 * miss. */
s32 formula_accuracy_calculate_physical_status(void) {
    formula_apply_attack_up_and_martial_arts();
    formula_apply_physical_modifiers_without_supports();
    formula_store_xa_plus_ya();
    formula_use_hp_damage_as_action_hit_percent();
    return g_action_target_data->hit == 0;
}

/* Formula 0x01: weapon damage (XA * YA by weapon type) with the weapon's
 * element; physical evade; 19% status. */
void formula_weapon_damage(void) {
    if (formula_evade_calculate_physical() == 0 && formula_calculate_weapon_damage() == 0) {
        formula_apply_status();
    }
}

/* Formula 0x02: as 0x01, but the 19% roll queues the weapon's proc ability on
 * the target instead of a status. */
void formula_weapon_damage_with_proc(void) {
    if (formula_evade_calculate_physical() == 0 && formula_calculate_weapon_damage() == 0) {
        formula_queue_weapon_spell();
    }
}

/* Formula 0x03: gun damage (WP * WP); no evade roll, element or status. */
void formula_gun_damage(void) {
    u16 weapon_power = g_action_work.weapon_data.power;

    g_action_work.xa = weapon_power;
    g_action_work.ya = weapon_power;
    formula_apply_charge();
    formula_calculate_physical_damage();
}

/* Formula 0x04: magic gun. Casts the first, second or third spell of the gun's
 * element (60/30/10%); damage F(WP * that spell's Y) with its element; no
 * evade. */
void formula_magic_gun(void) {
    u16 weapon_power;
    u16 ability_y;

    formula_select_magic_gun_ability();
    weapon_power = g_action_work.weapon_data.power;
    ability_y = g_action_work.range_data.y;
    g_action_work.xa = weapon_power;
    g_action_work.ya = ability_y;
    formula_apply_charge();
    formula_element_apply_weapon_strengthen();
    formula_apply_magical_modifiers();
    if (formula_calculate_elemental_xa_times_ya() == 0) {
        formula_calculate_faith();
        formula_element_apply_absorption();
    }
}

/* Formula 0x05: weapon damage as 0x01 without the weapon's element and the bow
 * weather penalty; physical evade; 19% status. No retail weapon, ability or item
 * record uses it: Charge runs the weapon's own formula (its menu case falls
 * through to Attack's), so it keeps the element and the bow weather penalty. */
void formula_weapon_damage_without_element_or_weather(void) {
    if (formula_evade_calculate_physical_without_weather() == 0) {
        formula_calculate_base_xa();
        formula_apply_charge();
        formula_calculate_physical_damage();
        if (formula_roll_conditional_status_proc() == 0) {
            formula_apply_status();
        }
    }
}

/* Formula 0x06: weapon damage (XA * YA), absorbed by the attacker as HP
 * (reversed against undead); physical evade; no element. */
void formula_weapon_absorb_hp(void) {
    if (formula_evade_calculate_physical() == 0) {
        formula_calculate_base_xa();
        formula_apply_charge();
        formula_calculate_physical_damage();
        formula_apply_hp_absorption();
    }
}

/* Formula 0x07 (Healing Staff): heal weapon (XA * YA) HP (damage to undead); no evade. Zodiac
 * compatibility is applied twice: here and again inside
 * formula_apply_physical_modifiers. */
void formula_healing_staff(void) {
    formula_calculate_base_xa();
    formula_apply_zodiac_compatibility();
    formula_apply_charge();
    formula_apply_physical_modifiers();
    formula_store_xa_times_ya_damage();
    formula_apply_undead_reversal();
}

/* Formula 0x08: damage F(MA * Y) with element; magic evade; 19% status. */
void formula_magical_damage(void) {
    if (formula_evade_calculate_magical() == 0) {
        formula_store_ma_and_y();
        formula_element_apply_strengthen();
        formula_apply_magical_modifiers();
        if (formula_calculate_elemental_xa_times_ya() == 0) {
            formula_calculate_faith();
            if (formula_element_apply_absorption_and_status_proc() == 0) {
                formula_apply_status();
            }
        }
    }
}

/* Formula 0x09: damage (Y)% of max HP with element, hit F(MA+X)%; magic evade;
 * 19% status. */
void formula_magic_hp_percent_damage(void) {
    if (formula_evade_calculate_magical() == 0) {
        if (formula_accuracy_calculate_magic() == 0) {
            formula_calculate_hp_percent_damage();
            formula_element_apply_weather();
            formula_element_apply();
            if (g_action_target_data->hit != 0) {
                if (formula_element_apply_absorption_and_status_proc() == 0) {
                    formula_apply_status();
                }
            }
        }
    }
}

/* Formula 0x0A: status, hit F(MA+X)%; magic evade. */
void formula_hit_faith_ma_x_percent(void) {
    if (formula_evade_calculate_magical() == 0 && formula_accuracy_calculate_magic() == 0) {
        formula_apply_status_to_action();
    }
}

/* Formula 0x0B: status, hit F(MA+X)%; no evade, and none of the target's magic
 * defenses. */
void formula_friendly_magic_accuracy(void) {
    formula_store_ma_and_x();
    formula_element_apply_strengthen();
    if (friendly_magic_accuracy_calculate() == 0) {
        formula_apply_status_to_action();
    }
}

/* Formula 0x0C: heal F(MA * Y) HP (damage to undead). */
void formula_magical_heal(void) {
    formula_store_ma_and_y();
    formula_element_apply_strengthen();
    formula_calculate_magical_xa_times_ya();
    formula_calculate_faith();
    formula_apply_undead_reversal();
}

/* Formula 0x0D: heal (Y)% of max HP, hit F(MA+X)%. The ability's status must
 * change something (so Raise fails on a living target) unless the target is
 * undead, which takes the amount as damage. */
void formula_heal_y_percent_faith(void) {
    formula_store_ma_and_x();
    formula_element_apply_strengthen();
    if (friendly_magic_accuracy_calculate() != 0) {
        return;
    }
    if (formula_apply_status_and_check_undead() == 0) {
        return;
    }
    formula_calculate_hp_percent_damage();
    formula_apply_undead_reversal();
}

/* Formula 0x0E: damage (Y)% of max HP, hit F(MA+X)%; magic evade. The
 * ability's status must apply. An undead target skips the evade and its magic
 * defenses and is healed instead; a target immune to Dead is missed. */
void formula_damage_hp_percent_hit_faith_ma_x_percent(void) {
    if (g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_UNDEAD)]
        & STATUS_BYTE_MASK(STATUS_ID_UNDEAD)) {
        g_action_work.target_is_undead = 1;
    } else {
        g_action_work.target_is_undead = 0;
        if (formula_evade_calculate_magical() != 0) {
            return;
        }
    }
    formula_store_ma_and_x();
    formula_element_apply_strengthen();
    formula_apply_magic_attack_up();
    if (g_action_work.target_is_undead == 0) {
        formula_apply_magic_defense_up();
        formula_apply_target_magical_statuses();
    }
    formula_apply_zodiac_compatibility();
    formula_store_xa_plus_ya();
    formula_calculate_faith();
    formula_use_hp_damage_as_action_hit_percent();
    if (g_action_target_data->hit == 0) {
        return;
    }
    if (formula_apply_status_and_check_undead() == 0) {
        return;
    }
    formula_calculate_hp_percent_damage();
    formula_apply_undead_absorb_attack();
}

/* Formula 0x0F: absorb (Y)% of max MP, hit F(MA+X)%; magic evade. */
void formula_absorb_mp_y_percent(void) {
    if (formula_evade_calculate_magical() != 0 || magic_accuracy_without_strengthen_calculate() != 0) {
        return;
    }
    formula_calculate_mp_percent_damage();
    /* formula_apply_mp_absorption takes its amount from hp_damage. */
    g_action_target_data->hp_damage = g_action_target_data->mp_damage;
    formula_apply_mp_absorption();
}

/* Formula 0x10: absorb (Y)% of max HP, hit F(MA+X)%; magic evade. */
void formula_magic_hp_absorb(void) {
    if (formula_evade_calculate_magical() == 0 && magic_accuracy_without_strengthen_calculate() == 0) {
        formula_calculate_hp_percent_damage();
        formula_apply_hp_absorption();
    }
}

/* Formula 0x11: an empty handler in g_formulas. */
void formula_11_unused(void) {
}

/* Formula 0x12: Quick, hit F(MA+X)%; no evade. */
void formula_set_quick(void) {
    formula_store_ma_and_x();
    if (friendly_magic_accuracy_calculate() == 0) {
        formula_apply_quick_effect();
    }
}

/* Formula 0x13: an empty handler in g_formulas. */
void formula_13_unused(void) {
}

/* Formula 0x14: Golem, hit ((MA+X) * caster Faith / 100)%. */
void formula_set_golem(void) {
    if (golem_accuracy_calculate() == 0) {
        /* The 16-bit store clears all other special-effect flags. */
        g_action_target_data->special_effect = ACTION_SPECIAL_EFFECT_SET_GOLEM;
        g_action_target_data->attack_type = ACTION_TYPE_PSEUDO_STATUS;
    }
}

/* Formula 0x15 (Return 2): CT to 0, hit F(MA+X)%; magic evade. */
void formula_return_2(void) {
    if (formula_evade_calculate_magical() == 0 && magic_accuracy_without_strengthen_calculate() == 0) {
        g_action_target_data->ct_change = ACTION_CT_CHANGE_ZERO;
        g_action_target_data->attack_type = ACTION_TYPE_PSEUDO_STATUS;
    }
}

/* Formula 0x16 (Mute): MP damage equal to the target's current MP, hit F(MA+X)%;
 * magic evade. */
void formula_mute(void) {
    action_data_t* action;
    u16 mp;

    if (formula_evade_calculate_magical() == 0 && magic_accuracy_without_strengthen_calculate() == 0) {
        action = g_action_target_data;
        mp = g_action_target->mp;
        action->attack_type = ACTION_TYPE_MP_DAMAGE;
        action->mp_damage = mp;
    }
}

/* Formula 0x17 (Gravi 2): damage equal to the target's current HP - 1, hit F(MA+X)%;
 * magic evade. */
void formula_gravi_2(void) {
    if (formula_evade_calculate_magical() == 0 && magic_accuracy_without_strengthen_calculate() == 0) {
        formula_calculate_damage_leaving_one_hp();
    }
}

/* Formula 0x18: an empty handler in g_formulas. */
void formula_18_unused(void) {
}

/* Formula 0x19: an empty handler in g_formulas. */
void formula_19_unused(void) {
}

/* Formula 0x1A (Destroy Sword stat ruins): -PA, -MA or -Speed (X) by ability, hit F(MA+Y)%; magic evade. */
void formula_destroy_sword_stat_ruin(void) {
    if (formula_evade_calculate_magical() == 0) {
        formula_store_ma_and_y();
        formula_element_apply_strengthen();
        formula_apply_magical_modifiers();
        formula_store_xa_plus_ya();
        formula_calculate_faith();
        formula_use_hp_damage_as_action_hit_percent();
        if (g_action_target_data->hit == 0) {
            /* The target clears the bit in the special_effect 16-bit value. */
            g_action_target_data->special_effect &= ~ACTION_SPECIAL_EFFECT_BREAK_EQUIPMENT;
        } else {
            formula_determine_reduced_stat();
        }
    }
}

/* Formula 0x1B (Magic Ruin): MP damage (Y)% of max MP, hit F(MA+X)%; magic evade. */
void formula_magic_ruin(void) {
    if (formula_evade_calculate_magical() == 0) {
        if (formula_accuracy_calculate_magic() == 0) {
            formula_calculate_mp_percent_damage();
        }
    }
}

/* Formula 0x1C: song, hit X%: heal (MA + Y) MP or HP, +1 Speed, PA or MA,
 * status, or Quick by ability. Never hits a sleeping target. */
void formula_song(void) {
    formula_force_sleeping_target_miss();
    if (g_action_target_data->hit != 0) {
        if (formula_calculate_dance_song_hit() == 0) {
            formula_store_ma_and_y();
            formula_apply_song_abilities();
        }
    }
}

/* Formula 0x1D: dance, hit X%: (weapon XA + YA) MP or HP damage, -1 Speed, PA
 * or MA, status, or CT to 0 by ability. Never hits a sleeping target. */
void formula_dance(void) {
    formula_force_sleeping_target_miss();
    if (g_action_target_data->hit != 0) {
        if (formula_calculate_dance_song_hit() == 0) {
            formula_calculate_base_xa();
            formula_apply_dance_abilities();
        }
    }
}

/* Formula 0x1E: damage ((MA + Y) / 2 * MA) with element; magic evade; 19%
 * status. Strikes rnd(1..X) times
 * (strike_data_init). */
void formula_multihit_truth_magic(void) {
    if (formula_evade_calculate_magical() == 0) {
        formula_calculate_truth_damage();
    }
}

/* Formula 0x1F (Un-truth): damage ((MA + Y) / 2 * MA) with element, scaled by (100 -
 * caster Faith) and (100 - target Faith) over 10000, the Faith status counting
 * as 0 and Innocent as 100, so Faith on either side keeps that side's full
 * factor and Innocent zeroes the damage; magic evade; 19% status. Strikes
 * rnd(1..X) times (strike_data_init). */
void formula_un_truth(void) {
    action_data_t* action;

    if (formula_evade_calculate_magical() != 0) {
        return;
    }
    formula_store_ma_and_ma_plus_y_divided_by_two();
    formula_element_apply_strengthen();
    formula_apply_magical_modifiers();
    if (formula_calculate_elemental_xa_times_ya() != 0) {
        return;
    }
    if (g_action_attacker->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FAITH)]
        & STATUS_BYTE_MASK(STATUS_ID_FAITH)) {
        g_action_work.attacker_faith = 0;
    }
    if (g_action_attacker->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_INNOCENT)]
        & STATUS_BYTE_MASK(STATUS_ID_INNOCENT)) {
        g_action_work.attacker_faith = 100;
    }
    if (g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FAITH)] & STATUS_BYTE_MASK(STATUS_ID_FAITH)) {
        g_action_work.target_faith = 0;
    }
    if (g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_INNOCENT)]
        & STATUS_BYTE_MASK(STATUS_ID_INNOCENT)) {
        g_action_work.target_faith = 100;
    }
    action = g_action_target_data;
    action->hp_damage
        = action->hp_damage * (100 - g_action_work.target_faith) * (100 - g_action_work.attacker_faith) / 10000;
    if (formula_element_apply_absorption_and_status_proc() == 0) {
        formula_apply_status();
    }
}

/* Formula 0x20: Draw Out damage (MA * Y) with element; no evade; 19% status;
 * katana break roll. */
void formula_draw_out_damage(void) {
    formula_calculate_katana_break_chance();
    formula_store_ma_and_y();
    formula_calculate_magical_damage_without_faith();
}

/* Formula 0x21 (Bizen Boat): Draw Out MP damage (MA * Y); katana break roll. */
void formula_bizen_boat(void) {
    s16 damage;
    action_data_t* action;

    formula_calculate_katana_break_chance();
    formula_store_ma_and_y();
    formula_apply_magical_modifiers();
    damage = g_action_work.xa * g_action_work.ya;
    action = g_action_target_data;
    action->attack_type = ACTION_TYPE_MP_DAMAGE;
    action->mp_damage = damage;
}

/* Formula 0x22: Draw Out status, 100%; katana break roll. */
void formula_draw_out_status(void) {
    formula_calculate_katana_break_chance();
    formula_apply_status_to_action();
}

/* Formula 0x23 (Murasame): Draw Out heal (MA * Y) HP with no modifiers (damage to
 * undead); katana break roll. */
void formula_murasame(void) {
    formula_calculate_katana_break_chance();
    formula_store_ma_and_y();
    formula_store_xa_times_ya_damage();
    formula_apply_undead_reversal();
}

/* Formula 0x24 (Elemental): damage ((PA + Y) / 2 * MA) with element; magic evade; 19%
 * status. */
void formula_elemental(void) {
    if (formula_evade_calculate_magical() == 0) {
        formula_store_ma_and_pa_plus_y_divided_by_two();
        formula_calculate_magical_damage_without_faith();
    }
}

/* Formula 0x25 (Battle Skill equipment breaks): break the equipment the ability names, hit (PA+WP+Y)%;
 * physical evade; Maintenance blocks it. With nothing in that slot the action
 * becomes a plain attack. */
void formula_battle_skill_equipment_break(void) {
    action_data_t* action;

    if (formula_select_targeted_equipment() != 0) {
        g_action_work.defaulted_to_attack = 1;
        action_switch_ability_to_default_attack();
        return;
    }
    g_action_work.defaulted_to_attack = 0;
    if (formula_evade_calculate_physical() != 0) {
        return;
    }
    formula_store_pa_and_weapon_power_plus_y();
    if (formula_accuracy_calculate_physical() != 0) {
        /* The target writes the combined special-effect field as one 16-bit value. */
        g_action_target_data->special_effect = 0;
        return;
    }
    formula_apply_maintenance();
    action = g_action_target_data;
    if (action->hit != 0) {
        action->special_effect = ACTION_SPECIAL_EFFECT_BREAK_EQUIPMENT;
    }
}

/* Formula 0x26: steal the equipment the ability names, hit (SP+X)%; physical
 * evade; Maintenance blocks it. An empty slot or a monster target is a forced
 * failure. */
void formula_steal_equipment(void) {
    action_data_t* action;

    if (formula_select_targeted_equipment() != 0) {
        formula_nullify_action();
        g_action_target_data->miss_type = ACTION_MISS_TYPE_FORCED_FAILURE;
        return;
    }
    if (formula_evade_calculate_physical() != 0) {
        return;
    }
    formula_store_speed_and_x();
    if (formula_accuracy_calculate_physical_status() != 0) {
        return;
    }
    formula_apply_maintenance();
    action = g_action_target_data;
    if (action->hit != 0) {
        /* The 16-bit store clears all other special-effect flags. */
        action->special_effect = ACTION_SPECIAL_EFFECT_STEAL_ITEM;
    }
}

/* Formula 0x27: steal (Level * SP) gil, hit (SP+X)%; physical evade. */
void formula_steal_gil(void) {
    if (formula_evade_calculate_physical() == 0) {
        formula_store_speed_and_x();
        if (formula_accuracy_calculate_physical_status() == 0) {
            formula_calculate_stolen_gil();
        }
    }
}

/* Formula 0x28 (Steal Exp.): steal (SP + Y) EXP, at most 100 and at most what the target
 * has, hit (SP+X)%; physical evade. */
void formula_steal_exp(void) {
    if (formula_evade_calculate_physical() == 0) {
        formula_store_speed_and_x();
        formula_apply_attack_up_and_martial_arts();
        formula_apply_attacker_berserk_frog();
        formula_apply_defense_up();
        formula_apply_target_physical_statuses();
        formula_apply_zodiac_compatibility();
        formula_store_xa_plus_ya();
        formula_use_hp_damage_as_action_hit_percent();
        /* Outside execution the amount is filled in even after a miss (a 0%
         * chance), so previews and the AI still see it. */
        if (g_action_target_data->hit != 0 || g_action_state != ACTION_STATE_EXECUTE) {
            formula_set_exp_stolen();
        }
    }
}

/* Formula 0x29: Charm, hit (MA+X)%. A target whose sex and monster
 * flags all equal the caster's is a forced miss, so a monster target of a
 * human caster can be hit. */
void formula_charm(void) {
    formula_store_ma_and_x();
    formula_apply_zodiac_compatibility();
    formula_store_xa_plus_ya();
    formula_use_hp_damage_as_action_hit_percent();
    if (g_action_target_data->hit != 0) {
        if ((g_action_target->unit_flags & (UNIT_FLAG_MONSTER | UNIT_FLAG_FEMALE | UNIT_FLAG_MALE))
            == (g_action_attacker->unit_flags & (UNIT_FLAG_MONSTER | UNIT_FLAG_FEMALE | UNIT_FLAG_MALE))) {
            formula_force_attack_miss();
        } else {
            formula_apply_status_to_action();
        }
    }
}

/* Formula 0x2A (Talk Skill), hit (MA+X)%: gil (Level * SP), CT to 0, +/-Brave
 * or Faith (Y), or status by ability (formula_apply_talk_skill). Misses
 * a sleeping target and, without Monster Talk, a monster; Finger Guard can
 * cancel it. */
void formula_talk_skill(void) {
    formula_force_sleeping_target_miss();
    if ((g_action_target->unit_flags & UNIT_FLAG_MONSTER)
        && !(g_action_attacker->ability_flags.support[2] & UNIT_SUPPORT_SET_3_MONSTER_TALK)) {
        formula_force_attack_miss();
    }
    if (g_action_target_data->hit != 0) {
        reaction_try_finger_guard();
        if (g_action_target_data->hit != 0) {
            formula_store_ma_and_x();
            formula_apply_zodiac_compatibility();
            formula_store_xa_plus_ya();
            formula_use_hp_damage_as_action_hit_percent();
            if (g_action_target_data->hit != 0) {
                formula_apply_talk_skill();
            }
        }
    }
}

/* Formula 0x2B (Battle Skill stat breaks): -PA, -MA or -Speed (X) by ability, hit (PA+Y)%; physical
 * evade. */
void formula_battle_skill_stat_break(void) {
    if (formula_evade_calculate_physical() == 0) {
        formula_store_pa_and_y();
        if (formula_accuracy_calculate_physical() == 0) {
            formula_determine_reduced_stat();
        }
    }
}

/* Formula 0x2C (Magic Break): MP damage (Y)% of max MP, hit (PA+Y)%; physical evade. */
void formula_magic_break(void) {
    if (formula_evade_calculate_physical() == 0) {
        formula_store_pa_and_y();
        if (formula_accuracy_calculate_physical() == 0) {
            formula_calculate_mp_percent_damage();
        }
    }
}

/* Formula 0x2D (Holy Sword): damage (PA * (WP + Y)) with the weapon's element, 100% status;
 * physical evade. */
void formula_holy_sword(void) {
    if (formula_evade_calculate_physical() == 0) {
        formula_store_pa_and_weapon_power_plus_y();
        formula_element_apply_weapon_strengthen();
        formula_calculate_physical_damage();
        formula_element_modify_weapon_damage();
        if (g_action_target_data->hit != 0) {
            formula_element_apply_absorption();
            formula_apply_status();
        }
    }
}

/* Formula 0x2E (Mighty Sword): break the equipment the ability names and damage (PA * WP)
 * with the weapon's element; physical evade. Fails with nothing in that slot
 * or against Maintenance. */
void formula_mighty_sword(void) {
    action_data_t* action;

    if (formula_evade_calculate_physical() != 0) {
        return;
    }
    if (formula_select_targeted_equipment() != 0) {
        formula_nullify_action();
        g_action_target_data->miss_type = ACTION_MISS_TYPE_FORCED_FAILURE;
        return;
    }
    formula_apply_maintenance();
    action = g_action_target_data;
    if (action->hit == 0) {
        return;
    }
    /* The target writes the combined special-effect field as one 16-bit value. */
    action->special_effect = ACTION_SPECIAL_EFFECT_BREAK_EQUIPMENT;
    formula_store_pa_and_weapon_power();
    formula_element_apply_weapon_strengthen();
    formula_calculate_physical_damage();
    formula_element_modify_weapon_damage();
    if (g_action_target_data->hit == 0) {
        return;
    }
    formula_element_apply_absorption();
}

/* Formula 0x2F (Dark Sword): absorb (PA * WP) MP; physical evade. */
void formula_dark_sword(void) {
    if (formula_evade_calculate_physical() == 0) {
        formula_store_pa_and_weapon_power();
        formula_calculate_physical_damage();
        formula_apply_mp_absorption();
    }
}

/* Formula 0x30 (Night Sword): absorb (PA * WP) HP; physical evade. */
void formula_night_sword(void) {
    if (formula_evade_calculate_physical() == 0) {
        formula_store_pa_and_weapon_power();
        formula_calculate_physical_damage();
        formula_apply_hp_absorption();
    }
}

/* Formula 0x31: damage ((PA + Y) / 2 * PA) with element; physical evade; 19%
 * status. */
void formula_damage_pa_plus_y_half_times_pa(void) {
    if (formula_evade_calculate_physical() == 0) {
        formula_store_pa_and_pa_plus_y_divided_by_two();
        formula_element_apply_strengthen();
        formula_apply_attack_up_and_martial_arts();
        formula_apply_physical_modifiers_without_supports();
        formula_calculate_critical_hit();
        formula_store_xa_times_ya_damage();
        formula_element_apply_weather();
        formula_element_apply();
        if (g_action_target_data->hit != 0) {
            formula_element_apply_absorption();
            if (formula_roll_conditional_status_proc() == 0) {
                formula_apply_status();
            }
        }
    }
}

/* Formula 0x32 (Repeating Fist): damage ((PA + (PA + Y) / 2) * rnd(1..X)); physical evade. The
 * sum comes from formula_store_xa_plus_ya, so zodiac compatibility
 * scales only its (PA + Y) / 2 part. The random factor is kept in
 * random_damage_factor. */
void formula_repeating_fist(void) {
    action_data_t* action;
    u8* factor;
    s32 rnd;

    if (formula_evade_calculate_physical() != 0)
        return;
    formula_store_pa_and_pa_plus_y_divided_by_two();
    formula_apply_attack_up_and_martial_arts();
    formula_apply_physical_modifiers_without_supports();
    formula_calculate_critical_hit();
    formula_store_xa_plus_ya();
    rnd = formula_get_random();
    factor = &g_action_work.random_damage_factor;
    *factor = (rnd * g_action_work.range_data.x) / 32768 + 1;
    action = g_action_target_data;
    /* The target loads hp_damage signed (lh) here. */
    action->hp_damage = action->hp_damage * *factor;
    action->attack_type = ACTION_TYPE_HP_DAMAGE;
}

/* Formula 0x33: hit (PA+X)%. */
void formula_stigma_magic(void) {
    formula_store_pa_and_x();
    formula_apply_attack_up_and_martial_arts();
    formula_apply_attacker_berserk_frog();
    formula_apply_zodiac_compatibility();
    formula_store_xa_plus_ya();
    formula_use_hp_damage_as_action_hit_percent();
    if (g_action_target_data->hit != 0) {
        formula_apply_status_to_action();
    }
}

/* Formula 0x34 (Chakra): heal (PA*Y) HP and (PA*Y/2) MP. */
void formula_chakra(void) {
    action_data_t* action;
    u16 amount;

    formula_store_pa_and_y();
    formula_apply_attack_up_and_martial_arts();
    formula_apply_zodiac_compatibility();
    formula_store_xa_times_ya_damage();
    action = g_action_target_data;
    amount = action->hp_damage;
    action->hp_damage = 0;
    action->hp_healing = amount;
    action->attack_type = ACTION_TYPE_HP_HEALING | ACTION_TYPE_MP_HEALING;
    /* hp_healing is re-read signed (lh) for the halving. */
    action->mp_healing = action->hp_healing / 2;
}

/* Formula 0x35: heal (Y)% of max HP, hit (PA+X)%. */
void formula_heal_y_percent_hit_pa_x_percent(void) {
    formula_store_pa_and_x();
    formula_apply_attack_up_and_martial_arts();
    formula_apply_zodiac_compatibility();
    formula_store_xa_plus_ya();
    formula_use_hp_damage_as_action_hit_percent();
    if (g_action_target_data->hit != 0 && formula_apply_status_and_check_undead() != 0) {
        formula_calculate_hp_percent_damage();
        formula_apply_undead_reversal();
    }
}

/* Formula 0x37: damage (PA * rnd(1..Y)); physical evade. Knockback when a roll
 * below the target's Brave is under a roll below the attacker's Brave. */
void formula_damage_random_y_times_pa(void) {
    if (formula_evade_calculate_physical() == 0) {
        formula_store_pa_and_y();
        formula_apply_attack_up_and_martial_arts();
        formula_apply_physical_modifiers_without_supports();
        formula_apply_damage_and_knockback();
    }
}

/* Formula 0x3D: status, hit (MA+X)%; magic evade. */
void formula_hit_ma_x_percent_magical_evade(void) {
    if (formula_evade_calculate_magical() == 0 && formula_accuracy_calculate_magic_without_faith() == 0) {
        formula_apply_status_to_action();
    }
}

/* Formula 0x3E: damage equal to the target's current HP - 1 (0 when it has none). */
void formula_damage_target_hp_minus_one_always_hit(void) {
    formula_calculate_damage_leaving_one_hp();
}

/* Formula 0x3F (Snipe aim abilities): hit (SP+X)%. */
void formula_snipe_aim(void) {
    formula_store_speed_and_x();
    formula_apply_physical_modifiers();
    formula_store_xa_plus_ya();
    formula_use_hp_damage_as_action_hit_percent();
    if (g_action_target_data->hit != 0) {
        formula_apply_status_to_action();
    }
}

/* Formula 0x40: undead targets only, hit (SP+X)%. */
void formula_seal_evil(void) {
    formula_store_speed_and_x();
    formula_apply_physical_modifiers();
    formula_store_xa_plus_ya();
    formula_use_hp_damage_as_action_hit_percent();
    if (g_action_target_data->hit != 0) {
        if ((g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_UNDEAD)]
                & STATUS_BYTE_MASK(STATUS_ID_UNDEAD))
            == 0) {
            formula_force_attack_miss();
        } else {
            formula_apply_status_to_action();
        }
    }
}

/* Formula 0x41: hit (MA+X)%; forced miss when caster and target share a zodiac sign. */
void formula_galaxy_stop(void) {
    if (formula_accuracy_calculate_magic_without_faith() == 0) {
        if (g_action_target->birthday.zodiac == g_action_attacker->birthday.zodiac) {
            formula_force_attack_miss();
        } else {
            formula_apply_status_to_action();
        }
    }
}

/* Formula 0x42 (Work): damage (PA * Y); the caster takes (PA * Y / X), so the
 * caster's action data is marked hit too. No evade. */
void formula_work(void) {
    s16 damage;
    action_data_t* action;

    formula_store_pa_and_y();
    formula_apply_zodiac_compatibility();
    damage = g_action_work.xa * g_action_work.ya;
    g_action_target_data->hp_damage = damage;
    damage = damage / g_action_work.range_data.x;
    /* One load of the attacker's record serves both stores; reading the global
     * for each store is three instructions longer. */
    action = g_action_attacker_data;
    action->hit = 1;
    action->hp_damage = damage;
    g_action_target_data->attack_type = ACTION_TYPE_HP_DAMAGE;
    g_action_attacker_data->attack_type = ACTION_TYPE_HP_DAMAGE;
}

/* Formula 0x46: an empty handler in g_formulas. */
void formula_46_unused(void) {
}

/* Formula 0x47 (Blood Suck): absorb (Y)% of max HP, 100% status. */
void formula_blood_suck(void) {
    formula_calculate_hp_percent_damage();
    formula_apply_hp_absorption();
    formula_apply_status();
}

/* Formula 0x48 (Potion): heal (Z * 10) HP, Z from the used item's data. The amount is
 * stored as damage; formula_apply_undead_reversal turns it into healing
 * unless the target is undead. */
void formula_potion(void) {
    g_action_target_data->hp_damage = g_item_secondary_data[g_action_work.used_item_id].z * 10;
    formula_apply_undead_reversal();
}

/* A second entry to formula 0x49's handler. The formula table does not point
 * here and no retail code calls it. */
void formula_49_unused_with_extra_steps(void) {
    formula_ether();
}

/* Formula 0x4A (Elixir): heal 100% HP and 100% MP. */
void formula_elixir(void) {
    formula_apply_full_hp_mp_heal();
    formula_apply_undead_reversal();
}

/* Formula 0x4B (Phoenix Down): heal rnd(1..Z) HP once its status applies; an undead target
 * takes its current HP as damage instead. */
void formula_phoenix_down(void) {
    unit_stats_t* unit;

    if (formula_apply_status_and_check_undead() == 0)
        return;
    unit = g_action_target;
    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_UNDEAD)] & STATUS_BYTE_MASK(STATUS_ID_UNDEAD)) != 0) {
        g_action_target_data->hp_damage = unit->hp;
    } else {
        g_action_target_data->hp_damage
            = (formula_get_random() * g_item_secondary_data[g_action_work.used_item_id].z) / 32768 + 1;
    }
    formula_apply_undead_reversal();
}

/* Formula 0x4C: heal (MA * Y) HP. */
void formula_heal_ma_times_y(void) {
    formula_store_ma_and_y();
    formula_calculate_magical_xa_times_ya();
    formula_apply_undead_reversal();
}

/* Formula 0x4D: absorb (Y)% of max HP, hit (MA+X)%; physical evade. */
void formula_absorb_hp_y_percent_hit_ma_x_percent(void) {
    if (formula_evade_calculate_physical() != 0) {
        return;
    }
    formula_store_ma_and_x();
    if (formula_accuracy_calculate_physical_status() != 0) {
        return;
    }
    formula_calculate_hp_percent_damage();
    formula_apply_hp_absorption();
}

/* Formula 0x4E: damage (MA * Y) with element, 19% status; magic evade. */
void formula_damage_ma_times_y(void) {
    if (formula_evade_calculate_magical() == 0) {
        formula_store_ma_and_y();
        formula_calculate_magical_damage_without_faith();
    }
}

/* Formula 0x4F: damage equal to the caster's missing HP, hit (MA+X)%; physical
 * evade. */
void formula_goblin_punch(void) {
    if (formula_evade_calculate_physical() == 0) {
        formula_store_ma_and_x();
        if (formula_accuracy_calculate_physical_status() == 0) {
            formula_damage_caster_missing_hp();
        }
    }
}

/* Formula 0x50: status, hit (MA+X)%; physical evade. */
void formula_hit_ma_x_percent_physical_evade(void) {
    if (formula_evade_calculate_physical() == 0) {
        formula_store_ma_and_x();
        if (formula_accuracy_calculate_physical_status() == 0) {
            formula_apply_status_to_action();
        }
    }
}

/* Formula 0x51: hit (MA+X)%. */
void formula_hit_ma_x_percent_no_evade(void) {
    formula_store_ma_and_x();
    formula_element_apply_strengthen();
    formula_apply_magic_attack_up();
    formula_apply_zodiac_compatibility();
    formula_store_xa_plus_ya();
    formula_use_hp_damage_as_action_hit_percent();
    if (g_action_target_data->hit != 0) {
        formula_apply_status_to_action();
    }
}

/* Formula 0x52 (Self-Destruct): damage equal to the caster's missing HP (max HP - HP), with
 * 100% status; the caster itself, when caught in the area, takes its own
 * current HP as damage and no status. */
void formula_self_destruct(void) {
    if (g_action_target != g_action_attacker) {
        unit_stats_t* caster = g_action_attacker;
        action_data_t* action = g_action_target_data;

        action->hp_damage = caster->max_hp - caster->hp;
        action->attack_type = ACTION_TYPE_HP_DAMAGE;
    } else {
        action_data_t* action = g_action_target_data;

        action->hp_damage = g_action_target->hp;
        action->attack_type = ACTION_TYPE_HP_DAMAGE;
    }
    if (g_action_target != g_action_attacker) {
        formula_apply_status();
    }
}

/* Formula 0x53: damage (Y)% of max HP with element, hit (MA+X)%, 19% status;
 * magic evade. */
void formula_damage_y_percent_hit_ma_x_percent(void) {
    if (formula_evade_calculate_magical() != 0) {
        return;
    }
    if (formula_accuracy_calculate_magic_without_faith() != 0) {
        return;
    }
    formula_calculate_hp_percent_damage();
    formula_element_apply_weather();
    formula_element_apply();
    if (g_action_target_data->hit != 0 && formula_element_apply_absorption_and_status_proc() == 0) {
        formula_apply_status();
    }
}

/* Formula 0x54: heal (MA * Y) MP. */
void formula_magic_spirit(void) {
    formula_store_ma_and_y();
    formula_calculate_magical_xa_times_ya();
    formula_convert_hp_damage_to_mp_recovery();
}

/* Formula 0x55: -PA (Y), hit (MA+X)%. */
void formula_beaking(void) {
    if (formula_evade_calculate_magical() != 0 || formula_accuracy_calculate_magic_without_faith() != 0) {
        return;
    }
    g_action_target_data->pa_change = g_action_work.range_data.y & ACTION_STAT_CHANGE_VALUE_MASK;
    g_action_target_data->attack_type = ACTION_TYPE_PSEUDO_STATUS;
}

/* Formula 0x56: -MA (Y), hit (MA+X)%. */
void formula_circle(void) {
    if (formula_evade_calculate_magical() != 0 || formula_accuracy_calculate_magic_without_faith() != 0) {
        return;
    }
    g_action_target_data->ma_change = g_action_work.range_data.y & ACTION_STAT_CHANGE_VALUE_MASK;
    g_action_target_data->attack_type = ACTION_TYPE_PSEUDO_STATUS;
}

/* Formula 0x57 (Please Eat): +1 level (a miss at level 99) and the ability's status on the
 * caster. A full HP and MP heal is stored first, but its action type is then
 * overwritten by the level change's. */
void formula_please_eat(void) {
    action_data_t* action;
    unit_stats_t* saved_unit;
    action_data_t* saved_action;
    unit_stats_t* attacker;
    action_data_t* attacker_action;

    formula_apply_full_hp_mp_heal();
    formula_apply_undead_reversal();
    if (g_action_target->level >= UNIT_LEVEL_MAX) {
        g_action_target_data->attack_type = 0;
        formula_force_attack_miss();
        return;
    }

    /* The status goes to the caster: the target globals point at the caster
     * for formula_apply_status_to_action, then are put back. */
    action = g_action_target_data;
    action->attack_type = ACTION_TYPE_PSEUDO_STATUS;
    saved_unit = g_action_target;
    saved_action = g_action_target_data;
    attacker = g_action_attacker;
    attacker_action = g_action_attacker_data;
    /* The target writes the combined special-effect field as one 16-bit value. */
    action->special_effect = ACTION_SPECIAL_EFFECT_LEVEL_UP;
    g_action_target = attacker;
    g_action_target_data = attacker_action;
    attacker_action->hit = 1;
    formula_apply_status_to_action();
    g_action_target = saved_unit;
    g_action_target_data = saved_action;
}

/* Formula 0x58 (Moldball Virus): hit (MA+X)%; no evade. A generic human or
 * monster target becomes a Morbol; an immortal, mounted or unique target, or
 * one already of the Morbol family, is a forced miss. */
void formula_moldball_virus(void) {
    action_data_t* action;
    unit_stats_t* target;

    formula_store_ma_and_x();
    formula_element_apply_strengthen();
    formula_apply_magic_attack_up();
    formula_apply_zodiac_compatibility();
    formula_store_xa_plus_ya();
    formula_use_hp_damage_as_action_hit_percent();
    action = g_action_target_data;
    if (action->hit == 0)
        return;
    target = g_action_target;
    if (!(target->team_flags & UNIT_TEAM_FLAG_IMMORTAL) && target->mount_info == 0
        && target->character_identity >= CHARACTER_IDENTITY_GENERIC_MALE
        && target->character_identity <= CHARACTER_IDENTITY_MONSTER
        && (target->job_id < JOB_ID_MORBOL || target->job_id > JOB_ID_MORBOL_FAMILY_LAST)) {
        /* The 16-bit store clears all other special-effect flags. */
        action->special_effect = ACTION_SPECIAL_EFFECT_MORBOL;
        action->attack_type = ACTION_TYPE_PSEUDO_STATUS;
        return;
    }
    formula_force_attack_miss();
}

/* Formula 0x59: -Level (1), hit (MA+X)%; magic evade. A level-1 target is a
 * forced miss that still carries the level-down effect. */
void formula_level_blast(void) {
    action_data_t* action;

    if (formula_evade_calculate_magical() == 0 && formula_accuracy_calculate_magic_without_faith() == 0) {
        if (g_action_target->level < 2) {
            formula_force_attack_miss();
        }
        action = g_action_target_data;
        /* The target writes the combined special-effect field as one 16-bit value. */
        action->special_effect = ACTION_SPECIAL_EFFECT_LEVEL_DOWN;
        action->attack_type = ACTION_TYPE_PSEUDO_STATUS;
    }
}

/* Formula 0x5A (Dragon Tame): status, 100%; dragons only (formula_check_dragon). */
void formula_dragon_tame(void) {
    formula_check_dragon();
    if (g_action_target_data->hit != 0) {
        formula_apply_status_to_action();
    }
}

/* Formula 0x5B (Dragon Care): dragons only; the caster loses MaxHP/5 and the target gains
 * twice that (formula 0x3C), then 100% status. Y is not read. */
void formula_dragon_care(void) {
    formula_check_dragon();
    if (g_action_target_data->hit != 0) {
        formula_sacrifice_hp_to_heal();
        formula_apply_status();
    }
}

/* Formula 0x5C (Dragon PowerUp): +Brave (X), +PA/MA/Speed (Y); dragons only. */
void formula_dragon_power_up(void) {
    formula_check_dragon();
    if (g_action_target_data->hit != 0) {
        formula_add_brave_x_stats_y();
    }
}

/* Formula 0x5D (Dragon LevelUp): Quick; dragons only. */
void formula_dragon_level_up(void) {
    formula_check_dragon();
    if (g_action_target_data->hit != 0) {
        formula_apply_quick_effect();
    }
}

/* Formula 0x5E: damage ((MA + Y) / 2 * MA) with element, 19% status; magic
 * evade. strike_data_init runs X+1 strikes; this handler calculates one. */
void formula_fixed_multihit_magic(void) {
    if (formula_evade_calculate_magical() == 0) {
        formula_calculate_truth_damage();
    }
}

/* Formula 0x5F: damage ((MA + Y) / 2 * MA) with element, 19% status; magic
 * evade. */
void formula_nanoflare(void) {
    if (formula_evade_calculate_magical() == 0) {
        formula_calculate_truth_damage();
    }
}

/* Formula 0x60: damage ((MA + Y) / 2 * MA) with element, 19% status; no evade. */
void formula_damage_ma_plus_y_half_times_ma_status(void) {
    formula_calculate_truth_damage();
}

/* Formula 0x61: -Brave (Y), hit F(MA+X)%; magic evade. */
void formula_lower_brave_y(void) {
    if (formula_evade_calculate_magical() == 0 && formula_accuracy_calculate_magic() == 0) {
        formula_set_brave_loss_y();
    }
}

/* Formula 0x62: -Brave (Y), hit (MA+X)%; magic evade. */
void formula_look_of_fright(void) {
    if (formula_evade_calculate_magical() == 0 && formula_accuracy_calculate_magic_without_faith() == 0) {
        formula_set_brave_loss_y();
    }
}

/* Formula 0x63: thrown weapon damage (SP * WP) with the weapon's element;
 * Catch, then physical evade. */
void formula_throw_damage(void) {
    reaction_try_catch();
    if (g_action_target_data->hit == 0) {
        return;
    }
    if (formula_evade_calculate_physical() != 0) {
        return;
    }
    g_action_work.xa = g_action_attacker->attributes[UNIT_ATTRIBUTE_SPEED];
    g_action_work.ya = g_action_work.weapon_data.power;
    formula_apply_physical_modifiers_without_supports();
    formula_store_xa_times_ya_damage();
    formula_element_modify_weapon_damage();
    if (g_action_target_data->hit == 0) {
        return;
    }
    formula_element_apply_absorption();
}

/* Formula 0x64: damage (PA * WP); with a spear (PA * 3/2 * WP); bare-handed
 * (PA * (PA * Brave / 100)). No evade roll here. */
void formula_jump_damage(void) {
    formula_store_jump_xa_ya();
    formula_apply_physical_modifiers_without_supports();
    formula_store_xa_times_ya_damage();
}
