#include "fft/battle_camera.h"
#include "fft/battle_map.h"
#include "psx/types.h"

/* Toggle the camera tilt and play its sound. */
void camera_toggle_tilt(void) {
    s32 target;

    target = g_map_tilt_target;
    if (target == MAP_TILT_LOW) {
        g_camera_tilt_action = CAMERA_STEP_INCREASE;
        g_map_tilt_target = MAP_TILT_HIGH;
    } else if (target == MAP_TILT_HIGH) {
        g_map_tilt_target = MAP_TILT_LOW;
        g_camera_tilt_action = CAMERA_STEP_DECREASE;
    } else if (target == 3) {
        g_camera_tilt_action = CAMERA_STEP_DECREASE;
        g_map_tilt_target = 4;
    } else if (target == 4) {
        g_map_tilt_target = MAP_TILT_LOW;
        g_camera_tilt_action = CAMERA_STEP_INCREASE;
    }
    g_camera_tilt_increment = 1;
    main_sound_play_sfx(MAIN_SFX_CAMERA_TILT);
}
