#include "fft/battle_camera.h"
#include "psx/pad.h"

/* L1 starts a quarter turn left and R1 one right, each with its sound, unless
 * a rotation is running. */
void camera_handle_rotation_input(void) {
    if (g_camera_rotation_action == CAMERA_ROTATION_NONE) {
        if (g_main_input_pressed_buttons & PSX_PAD_L1) {
            camera_start_rotation_left((g_camera_render_state.vy & 0xFE00) - 0x400);
            main_sound_play_sfx_find_channel(MAIN_SFX_CAMERA_ROTATE_LEFT);
        }
        if (g_main_input_pressed_buttons & PSX_PAD_R1) {
            camera_start_rotation_right((g_camera_render_state.vy & 0xFE00) + 0x400);
            main_sound_play_sfx_find_channel(MAIN_SFX_CAMERA_ROTATE_RIGHT);
        }
    }
}
