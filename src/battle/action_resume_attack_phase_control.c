#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_menu.h"
#include "psx/types.h"

/* Enter the resume-attack-phase state and show the last queued post-effect
 * message, if any, as a post-action result. Returns 1 after showing a message and
 * 0 when the queue is empty. */
s32 action_resume_attack_phase_control(void) {
    s32 idx;

    g_battle_game_state = BATTLE_GAME_STATE_RESUME_ATTACK_PHASE;
    g_main_system_animation_speed = 1;
    battle_unit_get_casting();
    idx = g_action_post_effect_msg_counter;
    if (idx != 0) {
        idx -= 1;
        g_action_post_effect_msg_counter = idx;
        menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_POST_ACTION_RESULT, g_action_post_effect_msgs[idx].code,
            g_action_post_effect_msgs[idx].unit, g_action_post_effect_msgs[idx].value, 1);
        g_action_post_action = 0;
        return 1;
    }
    return 0;
}
