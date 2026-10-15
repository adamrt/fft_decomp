#include "fft/battle_camera.h"
#include "fft/battle_map.h"
#include "fft/main.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Start a zoom toward ONE (mode 0) or 0xc00 (mode 1) from whichever side the
 * scale is on. No retail code calls it; the L2 input goes through
 * camera_toggle_zoom. */
void camera_zoom_map(s32 mode) {
    switch (mode) {
    case 0:
        if (g_camera_zoom.vx > ONE) {
            g_map_zoom_target = MAP_ZOOM_IN;
            g_camera_zoom_action = CAMERA_STEP_DECREASE;
            g_camera_zoom_increment = 4;
            main_sound_play_sfx(MAIN_SFX_CAMERA_ZOOM_OUT);
            return;
        }
        if (g_camera_zoom.vx < ONE) {
            g_map_zoom_target = MAP_ZOOM_IN;
            g_camera_zoom_action = CAMERA_STEP_INCREASE;
            g_camera_zoom_increment = 4;
            main_sound_play_sfx(MAIN_SFX_CAMERA_ZOOM_IN);
            return;
        }
        return;
    case 1:
        if (g_camera_zoom.vx > 0xC00) {
            g_map_zoom_target = MAP_ZOOM_OUT;
            g_camera_zoom_action = CAMERA_STEP_DECREASE;
            g_camera_zoom_increment = 4;
            main_sound_play_sfx(MAIN_SFX_CAMERA_ZOOM_OUT);
            return;
        }
        if (g_camera_zoom.vx < 0xC00) {
            g_map_zoom_target = MAP_ZOOM_OUT;
            g_camera_zoom_action = CAMERA_STEP_INCREASE;
            g_camera_zoom_increment = 4;
            main_sound_play_sfx(MAIN_SFX_CAMERA_ZOOM_IN);
            return;
        }
        return;
    }
}
