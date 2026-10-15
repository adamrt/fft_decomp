#include "fft/battle_camera.h"
#include "fft/battle_map.h"

/* Toggle the battle map zoom between "in" (target=1, action=2) and
   "out" (target=4, action=4), playing the corresponding zoom SFX. Any
   other current zoom target is left alone. */
void camera_toggle_zoom(void) {
    if (g_map_zoom_target == MAP_ZOOM_IN) {
        g_camera_zoom_action = CAMERA_STEP_DECREASE;
        g_map_zoom_target = MAP_ZOOM_OUT;
        main_sound_play_sfx(MAIN_SFX_CAMERA_ZOOM_OUT);
    } else if (g_map_zoom_target == MAP_ZOOM_OUT) {
        g_map_zoom_target = MAP_ZOOM_IN;
        g_camera_zoom_action = CAMERA_STEP_INCREASE;
        main_sound_play_sfx(MAIN_SFX_CAMERA_ZOOM_IN);
    }
    g_camera_zoom_increment = 4;
}
