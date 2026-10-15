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

/* Raise negative yaw by whole turns; retain positive values of ONE or greater. */
s32 camera_wrap_yaw_angle(void) {
    s32 yaw;

    yaw = g_script_variables[EVENT_SCRIPT_VAR_CAMERA_YAW];
    while (yaw < 0) {
        yaw += ONE;
    }
    return yaw;
}
