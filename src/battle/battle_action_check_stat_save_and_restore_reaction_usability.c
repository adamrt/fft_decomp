#include "fft/battle.h"
#include "psx/types.h"

/* Formula 7 never triggers these reactions. */
#define ABILITY_FORMULA_NO_STAT_SAVE_REACTION 7

/* Dispatch the first active stat-save or resource-restoring reaction. */
void battle_action_check_stat_save_and_restore_reaction_usability(void) {
    battle_stats_t* target;
    u8 reaction_flags;

    if (g_current_ability.formula == ABILITY_FORMULA_NO_STAT_SAVE_REACTION
        || battle_action_can_unit_react_1(g_battle_action_target) != 0) {
        return;
    }
    target = g_battle_action_target;
    reaction_flags = target->reaction_abilities[0];
    if (reaction_flags & BATTLE_REACTION_SET_1_PA_SAVE) {
        battle_action_check_pa_save_ma_save_speed_save_regenerator_auto_potion_gilgame_heart_usability(
            ABILITY_ID_REACTION_PA_SAVE);
    } else if (reaction_flags & BATTLE_REACTION_SET_1_MA_SAVE) {
        battle_action_check_pa_save_ma_save_speed_save_regenerator_auto_potion_gilgame_heart_usability(
            ABILITY_ID_REACTION_MA_SAVE);
    } else if (reaction_flags & BATTLE_REACTION_SET_1_SPEED_SAVE) {
        battle_action_check_pa_save_ma_save_speed_save_regenerator_auto_potion_gilgame_heart_usability(
            ABILITY_ID_REACTION_SPEED_SAVE);
    } else if (reaction_flags & BATTLE_REACTION_SET_1_REGENERATOR) {
        battle_action_check_pa_save_ma_save_speed_save_regenerator_auto_potion_gilgame_heart_usability(
            ABILITY_ID_REACTION_REGENERATOR);
    } else {
        reaction_flags = target->reaction_abilities[1];
        if (reaction_flags & BATTLE_REACTION_SET_2_HP_RESTORE) {
            battle_action_check_critical_quick_hp_restore_mp_restore_meatbone_slash_usability(
                ABILITY_ID_REACTION_HP_RESTORE);
        } else if (reaction_flags & BATTLE_REACTION_SET_2_MP_RESTORE) {
            battle_action_check_critical_quick_hp_restore_mp_restore_meatbone_slash_usability(
                ABILITY_ID_REACTION_MP_RESTORE);
        } else if (reaction_flags & BATTLE_REACTION_SET_2_CRITICAL_QUICK) {
            battle_action_check_critical_quick_hp_restore_mp_restore_meatbone_slash_usability(
                ABILITY_ID_REACTION_CRITICAL_QUICK);
        } else if (reaction_flags & BATTLE_REACTION_SET_2_MEATBONE_SLASH) {
            battle_action_check_critical_quick_hp_restore_mp_restore_meatbone_slash_usability(
                ABILITY_ID_REACTION_MEATBONE_SLASH);
        } else {
            reaction_flags = target->reaction_abilities[2];
            if (reaction_flags & BATTLE_REACTION_SET_3_GILGAME_HEART) {
                battle_action_check_pa_save_ma_save_speed_save_regenerator_auto_potion_gilgame_heart_usability(
                    ABILITY_ID_REACTION_GILGAME_HEART);
            } else if (reaction_flags & BATTLE_REACTION_SET_3_AUTO_POTION) {
                battle_action_check_pa_save_ma_save_speed_save_regenerator_auto_potion_gilgame_heart_usability(
                    ABILITY_ID_REACTION_AUTO_POTION);
            }
        }
    }
}
