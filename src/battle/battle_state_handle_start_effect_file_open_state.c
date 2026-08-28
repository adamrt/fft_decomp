#include "fft/battle.h"
#include "fft/battle_effect.h"
#include "fft/battle_state.h"
#include "psx/types.h"

void battle_state_handle_start_effect_file_open_state(void) {
    /* The target loads no argument; the parameter is the $a0 value unhandled states return. */
    if (((s32 (*)(void))battle_effect_init_data)() != 0) {
        g_battle_state_animation_continue_check = 1;
    } else {
        g_battle_state_animation_continue_check = 0;
    }
    if (g_battle_state_animation_continue_check == 0) {
        battle_action_set_target_coords_and_attacker_anim();
    }
    battle_state_handle_free_cursor_input();
}
