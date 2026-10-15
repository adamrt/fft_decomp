#include "fft/battle_camera.h"
#include "fft/battle_event.h"
#include "fft/battle_map.h"
#include "fft/battle_script.h"
#include "fft/main.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Reduce the yaw script variable to one turn, preserving the original whole-turn addition. */
void camera_normalize_yaw_angle(void) {
    g_script_variables[EVENT_SCRIPT_VAR_CAMERA_YAW]
        = (g_script_variables[EVENT_SCRIPT_VAR_CAMERA_YAW] + 0xa000) & 0x0fff;
}
