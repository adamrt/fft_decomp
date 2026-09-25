#include "fft/battle.h"
#include "psx/pad.h"
#include "psx/types.h"

void battle_camera_call_toggle_tilt(void) {
    if (g_battle_camera_tilt_action == 0) {
        if ((g_controller_input_pressed & PSX_PAD_R2) != 0) {
            /* The target calls the two-parameter callee without loading arguments. */
            ((void (*)(void))battle_camera_toggle_tilt)();
        }
    }
}
