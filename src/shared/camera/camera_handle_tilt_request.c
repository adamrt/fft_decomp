#include "fft/battle_camera.h"
#include "fft/battle_map.h"
#include "fft/main.h"
#include "psx/types.h"

/* Start a tilt toward the low pitch 0x12e (mode 0) or the high pitch 0x1c0
 * (mode 1) from whichever side the pitch is on. No retail code calls it; the
 * R2 input goes through camera_toggle_tilt. */
void camera_handle_tilt_request(s32 mode) {
    switch (mode) {
    case 0:
        if (g_camera_render_state.vx >= 0x12F) {
            g_map_tilt_target = MAP_TILT_LOW;
            g_camera_tilt_action = CAMERA_STEP_DECREASE;
            g_camera_tilt_increment = 1;
            main_sound_play_sfx(MAIN_SFX_CAMERA_TILT);
        } else if (g_camera_render_state.vx < 0x12E) {
            g_map_tilt_target = MAP_TILT_LOW;
            g_camera_tilt_action = CAMERA_STEP_INCREASE;
            g_camera_tilt_increment = 1;
            main_sound_play_sfx(MAIN_SFX_CAMERA_TILT);
        }
        break;
    case 1:
        if (g_camera_render_state.vx >= 0x1C1) {
            g_map_tilt_target = MAP_TILT_HIGH;
            g_camera_tilt_action = CAMERA_STEP_DECREASE;
            g_camera_tilt_increment = mode;
            main_sound_play_sfx(MAIN_SFX_CAMERA_TILT);
        } else if (g_camera_render_state.vx < 0x1C0) {
            g_camera_tilt_action = CAMERA_STEP_INCREASE;
            g_map_tilt_target = MAP_TILT_HIGH;
            g_camera_tilt_increment = mode;
            main_sound_play_sfx(MAIN_SFX_CAMERA_TILT);
        }
        break;
    }
}
