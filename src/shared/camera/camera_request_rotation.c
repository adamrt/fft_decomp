#include "fft/battle_camera.h"
#include "psx/types.h"

/* Start an automatic turn of angle yaw units (0x400 is a quarter turn;
 * negative turns left) with its sound, unless a rotation is running. */
void camera_request_rotation(s32 angle) {
    if (g_camera_rotation_action == CAMERA_ROTATION_NONE) {
        if (angle < 0) {
            camera_start_rotation_left((g_camera_render_state.vy & 0xFE00) + angle);
            main_sound_play_sfx_find_channel(MAIN_SFX_CAMERA_ROTATE_LEFT);
        } else {
            camera_start_rotation_right((g_camera_render_state.vy & 0xFE00) + angle);
            main_sound_play_sfx_find_channel(MAIN_SFX_CAMERA_ROTATE_RIGHT);
        }
    }
}
