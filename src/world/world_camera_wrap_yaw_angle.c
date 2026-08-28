#include "fft/script_variables.h"
#include "fft/world.h"

/* Camera yaw normalised into 0..0xfff. */
s32 world_camera_wrap_yaw_angle(void) {
    s32 value = g_world_script_variables[EVENT_SCRIPT_VAR_CAMERA_YAW];
    while (value < 0) {
        value += ONE;
    }
    return value;
}
