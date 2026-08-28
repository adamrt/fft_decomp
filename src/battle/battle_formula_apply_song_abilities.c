#include "fft/battle.h"
#include "fft/data.h"
#include "psx/types.h"

void battle_formula_apply_song_abilities(void) {
    switch (g_current_ability.ability_id) {
    case ABILITY_ID_SONG_ANGEL_SONG:
        g_battle_action_target_data->mp_healing = g_current_ability.xa + g_current_ability.ya;
        g_battle_action_target_data->attack_type = BATTLE_ACTION_TYPE_MP_HEALING;
        return;
    case ABILITY_ID_SONG_LIFE_SONG:
        g_battle_action_target_data->hp_healing = g_current_ability.xa + g_current_ability.ya;
        g_battle_action_target_data->attack_type = BATTLE_ACTION_TYPE_HP_HEALING;
        return;
    case ABILITY_ID_SONG_CHEER_SONG:
        g_battle_action_target_data->sp_change = BATTLE_ACTION_STAT_CHANGE_INCREASE | 1;
        break;
    case ABILITY_ID_SONG_BATTLE_SONG:
        g_battle_action_target_data->pa_change = BATTLE_ACTION_STAT_CHANGE_INCREASE | 1;
        break;
    case ABILITY_ID_SONG_MAGIC_SONG:
        g_battle_action_target_data->ma_change = BATTLE_ACTION_STAT_CHANGE_INCREASE | 1;
        break;
    case ABILITY_ID_SONG_NAMELESS_SONG:
        battle_formula_apply_status_to_action();
        return;
    case ABILITY_ID_SONG_LAST_SONG:
        g_battle_action_target_data->ct_change = 0xFF;
        break;
    }
    g_battle_action_target_data->attack_type = BATTLE_ACTION_TYPE_PSEUDO_STATUS;
}
