#include "fft/battle.h"
#include "psx/types.h"

/* Formula 0x58 "Set Morbol: Hit(MA+X)%". */

/* Generic character identities are 0x80 male, 0x81 female, 0x82 monster;
 * Morbol-type jobs are 0x82 Morbol, 0x83 Ochu, 0x84 Great Morbol; the target
 * biases the job test with +0x7e rather than -0x82. */
void battle_formula_set_morbol(void) {
    battle_action_data_t* action;
    battle_stats_t* target;

    battle_formula_store_ma_and_x();
    battle_formula_apply_elemental_strengthen();
    battle_formula_apply_magic_attack_up();
    battle_formula_apply_zodiac_compatibility();
    battle_formula_store_xa_plus_ya_status_damage();
    battle_formula_use_hp_damage_as_action_hit_percent();
    action = g_battle_action_target_data;
    if (action->hit == 0)
        return;
    target = g_battle_action_target;
    if (!(target->team_flags & BATTLE_TEAM_FLAG_IMMORTAL) && target->mount_info == 0
        && (u32)((target->character_identity - CHARACTER_IDENTITY_SELECTOR_FIRST) & 0xFF)
            < CHARACTER_IDENTITY_SELECTOR_COUNT
        && (u32)((target->job_id + (0x100 - JOB_ID_MORBOL)) & 0xFF) >= JOB_ID_MORBOL_FAMILY_COUNT) {
        /* The halfword store clears all other special-effect flags. */
        action->special_effect = BATTLE_ACTION_SPECIAL_EFFECT_MORBOL;
        action->attack_type = BATTLE_ACTION_TYPE_PSEUDO_STATUS;
        return;
    }
    battle_formula_force_attack_miss();
}
