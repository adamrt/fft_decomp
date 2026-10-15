#include "fft/battle.h"
#include "psx/types.h"

/* StartEffectFileOpen game-state handler: step the effect-file load
 * (effect_init_data); when no work remains, resolve the strike and pick
 * the attacker's animation. */
void battle_state_handle_start_effect_file_open_state(void) {
    if (effect_init_data() != 0) {
        g_battle_state_animation_continue_check = 1;
    } else {
        g_battle_state_animation_continue_check = 0;
    }
    if (g_battle_state_animation_continue_check == 0) {
        action_set_target_coords_and_attacker_anim();
    }
    battle_state_handle_free_cursor_input();
}
