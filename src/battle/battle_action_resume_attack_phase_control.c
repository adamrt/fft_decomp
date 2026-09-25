#include "fft/battle.h"
#include "psx/types.h"

s32 battle_action_resume_attack_phase_control(void) {
    s32 idx;

    g_battle_game_state = BATTLE_GAME_STATE_RESUME_ATTACK_PHASE;
    g_animation_speed = 1;
    battle_unit_get_casting_misc_data();
    idx = g_battle_action_post_effect_msg_counter;
    if (idx != 0) {
        idx -= 1;
        g_battle_action_post_effect_msg_counter = idx;
        battle_menu_init_system_function(0x11, g_battle_action_post_effect_msgs[idx].code,
            g_battle_action_post_effect_msgs[idx].unit, g_battle_action_post_effect_msgs[idx].value, 1);
        g_battle_action_post_action = 0;
        return 1;
    }
    return 0;
}
