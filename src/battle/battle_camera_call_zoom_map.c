#include "fft/battle.h"
#include "fft/main_runtime.h"
#include "psx/pad.h"
#include "psx/types.h"

void battle_camera_call_zoom_map(void) {
    if (g_battle_camera_zoom_action == 0) {
        if ((g_controller_input_pressed & PSX_PAD_L2) != 0) {
            battle_camera_toggle_zoom();
        }
    }
}
