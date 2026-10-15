#include "fft/battle_camera.h"
#include "psx/pad.h"

/* Toggles the zoom when L2 is newly pressed and no zoom is running. */
void camera_update_zoom_input(void) {
    if (g_camera_zoom_action == CAMERA_STEP_NONE) {
        if ((g_main_input_pressed_buttons & PSX_PAD_L2) != 0) {
            camera_toggle_zoom();
        }
    }
}
