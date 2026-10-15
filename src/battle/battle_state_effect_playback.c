/* Effect playback entry, continue check and frame pacing reset. */

#include "fft/battle.h"
#include "psx/types.h"

/* Nonzero while an effect cycle or a unit step is still running; the event script
 * polls it to wait for an effect to end. */
s32 battle_state_get_animation_continue_check(void) {
    return g_battle_state_animation_continue_check;
}

/* Enter the effect-playback game state, saving the state and animation speed
 * that battle_state_handle_effect_state restores. Returns 1 when the effect data is
 * not ready yet and the caller must retry. No code calls it. */
s32 battle_state_enter_effect_playback(void) {
    s32 previous_state;
    s32 previous_animation_speed;

    /* The target loads no argument; the parameter is the $a0 value unhandled states return. */
    if (effect_init_data() != 0) {
        return 1;
    }
    effect_play();
    previous_state = g_battle_game_state;
    previous_animation_speed = g_main_system_animation_speed;
    g_battle_game_state = BATTLE_GAME_STATE_EFFECT;
    g_main_system_animation_speed = 2;
    g_battle_gfx_screen_color_modulation_fade_frames = 0;
    g_camera_screen_offset_countdown = 0;
    g_camera_rotation_countdown = 0;
    g_camera_zoom_countdown = 0;
    g_battle_previous_game_state = previous_state;
    g_main_saved_animation_speed = previous_animation_speed;
    return 0;
}

void battle_noop_8008de88(void) {
}

/* Stop the slow-down timer the game loop starts when a frame of the effect or
 * action-execute state takes more than 0x200 hsyncs. */
void battle_state_reset_frame_pacing_timer(void) {
    g_battle_frame_pacing_timer = 0;
}
