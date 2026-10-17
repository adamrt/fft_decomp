/* Ability formulas for magic accuracy, weapon and gun damage, magical damage and healing, quick, golem, CT zero, songs,
 * dances, truth, draw out, break and steal. */

#include "fft/ability.h"
#include "fft/action.h"
#include "fft/formula.h"
#include "fft/item.h"
#include "fft/status.h"
#include "fft/unit.h"
#include "psx/types.h"

/* Hit chance F(XA+YA)% for magic on allies: Magic Attack Up and zodiac
 * compatibility, none of the target's defenses. Returns 1 on a miss. */
static s32 formula_accuracy_calculate_friendly_magic(void) {
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
static s32 formula_accuracy_calculate_golem(void) {
    action_data_t* action;

    formula_store_ma_and_x();
    formula_apply_magic_attack_up();
    formula_store_xa_plus_ya();
    if ((g_action_attacker->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FAITH)] & STATUS_BYTE_MASK(STATUS_ID_FAITH))
        != 0) {
        g_action_current_ability.attacker_faith = 100;
    }
    if ((g_action_attacker->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_INNOCENT)]
            & STATUS_BYTE_MASK(STATUS_ID_INNOCENT))
        != 0) {
        g_action_current_ability.attacker_faith = 0;
    }
    action = g_action_target_data;
    /* The target reads hp_damage signed (lh). */
    action->hp_damage = action->hp_damage * g_action_current_ability.attacker_faith / 100;
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
static s32 formula_accuracy_calculate_magic_without_strengthen(void) {
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
    if (formula_evade_calculate_physical() == 0) {
        if (formula_calculate_weapon_damage() == 0) {
            formula_apply_status();
        }
    }
}

/* Formula 0x02: as 0x01, but the 19% roll queues the weapon's proc ability on
 * the target instead of a status. */
void formula_weapon_damage_with_proc(void) {
    if (formula_evade_calculate_physical() == 0) {
        if (formula_calculate_weapon_damage() == 0) {
            formula_queue_weapon_spell();
        }
    }
}

/* Formula 0x03: gun damage (WP * WP); no evade roll, element or status. */
void formula_gun_damage(void) {
    u16 weapon_power = g_action_current_ability.weapon_data.power;

    g_action_current_ability.xa = weapon_power;
    g_action_current_ability.ya = weapon_power;
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
    weapon_power = g_action_current_ability.weapon_data.power;
    ability_y = g_action_current_ability.range_data.y;
    g_action_current_ability.xa = weapon_power;
    g_action_current_ability.ya = ability_y;
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

/* Formula 0x07: heal weapon (XA * YA) HP (damage to undead); no evade. Zodiac
 * compatibility is applied twice: here and again inside
 * formula_apply_physical_modifiers. */
void formula_weapon_heal(void) {
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
    if (formula_evade_calculate_magical() == 0) {
        if (formula_accuracy_calculate_magic() == 0) {
            formula_apply_status_to_action();
        }
    }
}

/* Formula 0x0B: status, hit F(MA+X)%; no evade, and none of the target's magic
 * defenses. */
void formula_friendly_magic_accuracy(void) {
    formula_store_ma_and_x();
    formula_element_apply_strengthen();
    if (formula_accuracy_calculate_friendly_magic() == 0) {
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
    if (formula_accuracy_calculate_friendly_magic() != 0) {
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
        g_action_current_ability.target_is_undead = 1;
    } else {
        g_action_current_ability.target_is_undead = 0;
        if (formula_evade_calculate_magical() != 0) {
            return;
        }
    }
    formula_store_ma_and_x();
    formula_element_apply_strengthen();
    formula_apply_magic_attack_up();
    if (g_action_current_ability.target_is_undead == 0) {
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
    if (formula_evade_calculate_magical() != 0 || formula_accuracy_calculate_magic_without_strengthen() != 0) {
        return;
    }
    formula_calculate_mp_percent_damage();
    /* formula_apply_mp_absorption takes its amount from hp_damage. */
    g_action_target_data->hp_damage = g_action_target_data->mp_damage;
    formula_apply_mp_absorption();
}

/* Formula 0x10: absorb (Y)% of max HP, hit F(MA+X)%; magic evade. */
void formula_magic_hp_absorb(void) {
    if (formula_evade_calculate_magical() == 0) {
        if (formula_accuracy_calculate_magic_without_strengthen() == 0) {
            formula_calculate_hp_percent_damage();
            formula_apply_hp_absorption();
        }
    }
}

/* Formula 0x11: an empty handler in g_formula_handlers. */
void formula_11_unused(void) {
}

/* Formula 0x12: Quick, hit F(MA+X)%; no evade. */
void formula_set_quick(void) {
    formula_store_ma_and_x();
    if (formula_accuracy_calculate_friendly_magic() == 0) {
        formula_apply_quick_effect();
    }
}

/* Formula 0x13: an empty handler in g_formula_handlers. */
void formula_13_unused(void) {
}

/* Formula 0x14: Golem, hit ((MA+X) * caster Faith / 100)%. */
void formula_set_golem(void) {
    if (formula_accuracy_calculate_golem() == 0) {
        /* The 16-bit store clears all other special-effect flags. */
        g_action_target_data->special_effect = ACTION_SPECIAL_EFFECT_SET_GOLEM;
        g_action_target_data->attack_type = ACTION_TYPE_PSEUDO_STATUS;
    }
}

/* Formula 0x15: CT to 0, hit F(MA+X)%; magic evade. */
void formula_set_ct_zero(void) {
    if (formula_evade_calculate_magical() == 0) {
        if (formula_accuracy_calculate_magic_without_strengthen() == 0) {
            g_action_target_data->ct_change = ACTION_CT_CHANGE_ZERO;
            g_action_target_data->attack_type = ACTION_TYPE_PSEUDO_STATUS;
        }
    }
}

/* Formula 0x16: MP damage equal to the target's current MP, hit F(MA+X)%;
 * magic evade. */
void formula_damage_mp_target_current_mp(void) {
    unit_stats_t* target;
    action_data_t* action;
    u16 mp;

    if (formula_evade_calculate_magical() == 0) {
        if (formula_accuracy_calculate_magic_without_strengthen() == 0) {
            target = g_action_target;
            action = g_action_target_data;
            mp = target->mp;
            action->attack_type = ACTION_TYPE_MP_DAMAGE;
            action->mp_damage = mp;
        }
    }
}

/* Formula 0x17: damage equal to the target's current HP - 1, hit F(MA+X)%;
 * magic evade. */
void formula_damage_target_hp_minus_one(void) {
    if (formula_evade_calculate_magical() == 0) {
        if (formula_accuracy_calculate_magic_without_strengthen() == 0) {
            formula_calculate_damage_leaving_one_hp();
        }
    }
}

/* Formula 0x18: an empty handler in g_formula_handlers. */
void formula_18_unused(void) {
}

/* Formula 0x19: an empty handler in g_formula_handlers. */
void formula_19_unused(void) {
}

/* Formula 0x1A: -PA, -MA or -Speed (X) by ability, hit F(MA+Y)%; magic evade. */
void formula_magical_stat_reduction(void) {
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

/* Formula 0x1B: MP damage (Y)% of max MP, hit F(MA+X)%; magic evade. */
void formula_damage_mp_percent(void) {
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
 * (action_init_strike_data). */
void formula_multihit_truth_magic(void) {
    if (formula_evade_calculate_magical() == 0) {
        formula_calculate_truth_damage();
    }
}

/* Formula 0x1F: damage ((MA + Y) / 2 * MA) with element, scaled by (100 -
 * caster Faith) and (100 - target Faith) over 10000, the Faith status counting
 * as 0 and Innocent as 100, so Faith on either side keeps that side's full
 * factor and Innocent zeroes the damage; magic evade; 19% status. Strikes
 * rnd(1..X) times (action_init_strike_data). */
void formula_unfaith_magical_damage(void) {
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
        g_action_current_ability.attacker_faith = 0;
    }
    if (g_action_attacker->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_INNOCENT)]
        & STATUS_BYTE_MASK(STATUS_ID_INNOCENT)) {
        g_action_current_ability.attacker_faith = 100;
    }
    if (g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FAITH)] & STATUS_BYTE_MASK(STATUS_ID_FAITH)) {
        g_action_current_ability.target_faith = 0;
    }
    if (g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_INNOCENT)]
        & STATUS_BYTE_MASK(STATUS_ID_INNOCENT)) {
        g_action_current_ability.target_faith = 100;
    }
    action = g_action_target_data;
    action->hp_damage = action->hp_damage * (100 - g_action_current_ability.target_faith)
        * (100 - g_action_current_ability.attacker_faith) / 10000;
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

/* Formula 0x21: Draw Out MP damage (MA * Y); katana break roll. */
void formula_draw_out_mp_damage(void) {
    s16 damage;
    action_data_t* action;

    formula_calculate_katana_break_chance();
    formula_store_ma_and_y();
    formula_apply_magical_modifiers();
    damage = g_action_current_ability.xa * g_action_current_ability.ya;
    action = g_action_target_data;
    action->attack_type = ACTION_TYPE_MP_DAMAGE;
    action->mp_damage = damage;
}

/* Formula 0x22: Draw Out status, 100%; katana break roll. */
void formula_draw_out_status(void) {
    formula_calculate_katana_break_chance();
    formula_apply_status_to_action();
}

/* Formula 0x23: Draw Out heal (MA * Y) HP with no modifiers (damage to
 * undead); katana break roll. */
void formula_draw_out_heal(void) {
    formula_calculate_katana_break_chance();
    formula_store_ma_and_y();
    formula_store_xa_times_ya_damage();
    formula_apply_undead_reversal();
}

/* Formula 0x24: damage ((PA + Y) / 2 * MA) with element; magic evade; 19%
 * status. */
void formula_ma_pa_half_damage(void) {
    if (formula_evade_calculate_magical() == 0) {
        formula_store_ma_and_pa_plus_y_divided_by_two();
        formula_calculate_magical_damage_without_faith();
    }
}

/* Formula 0x25: break the equipment the ability names, hit (PA+WP+Y)%;
 * physical evade; Maintenance blocks it. With nothing in that slot the action
 * becomes a plain attack. */
void formula_break_equipped_hit_pa_wp_y_percent(void) {
    action_data_t* action;

    if (formula_select_targeted_equipment() != 0) {
        g_action_current_ability.defaulted_to_attack = 1;
        action_switch_ability_to_default_attack();
        return;
    }
    g_action_current_ability.defaulted_to_attack = 0;
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

/* Formula 0x28: steal (SP + Y) EXP, at most 100 and at most what the target
 * has, hit (SP+X)%; physical evade. */
void formula_steal_exp_hit_sp_x_percent(void) {
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

/* Formula 0x29: opposite sex only, hit (MA+X)%. A target whose sex and monster
 * flags all equal the caster's is a forced miss, so a monster target of a
 * human caster can be hit. */
void formula_opposite_sex_hit_ma_x_percent(void) {
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
