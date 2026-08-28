#include "fft/script_variables.h"
#include "fft/world.h"
#include "psx/types.h"

void world_camera_init_yaw_angle(void) {
    g_world_script_variables[EVENT_SCRIPT_VAR_CAMERA_YAW]
        = (g_world_script_variables[EVENT_SCRIPT_VAR_CAMERA_YAW] + 0xa000) & 0x0fff;
}
