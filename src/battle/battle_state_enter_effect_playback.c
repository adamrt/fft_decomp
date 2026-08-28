#include "fft/battle.h"
#include "fft/battle_effect.h"
#include "fft/main_runtime.h"

/* Enter the effect-playback game state, saving the state and animation speed
 * that battle_state_handle_effect_state restores. Returns 1 when the effect data is
 * not ready yet and the caller must retry. */
s32 battle_state_enter_effect_playback(void) {
    s32 previous_state;
    s32 previous_animation_speed;

    /* The target loads no argument; the parameter is the $a0 value unhandled states return. */
    if (((s32 (*)(void))battle_effect_init_data)() != 0) {
        return 1;
    }
    battle_effect_play();
    previous_state = g_battle_game_state;
    previous_animation_speed = g_animation_speed;
    g_battle_game_state = BATTLE_GAME_STATE_EFFECT;
    g_animation_speed = 2;
    g_battle_gfx_screen_color_modulation_fade_frames = 0;
    g_battle_camera_offset_screen_coord_countdown = 0;
    g_battle_camera_rotation_countdown = 0;
    g_battle_camera_zoom_countdown = 0;
    g_previous_battle_game_state = previous_state;
    g_main_saved_animation_speed = previous_animation_speed;
    return 0;
}
