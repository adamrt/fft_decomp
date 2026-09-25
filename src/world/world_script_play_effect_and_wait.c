#include "fft/battle_effect.h"
#include "fft/battle_state.h"
#include "fft/world.h"
#include "psx/types.h"

/* Runs the effect playback helper on the main stack, then yields once per
 * frame until the animation continue check reports completion. */
void world_script_play_effect_and_wait(void) {
    g_world_thread_inner_subroutine_callback = battle_effect_call_play;
    world_thread_call_on_main_stack();
    do {
        world_thread_yield();
        g_world_thread_inner_subroutine_callback = (void (*)(void))battle_state_get_animation_continue_check;
    } while (world_thread_call_on_main_stack() != 0);
}
