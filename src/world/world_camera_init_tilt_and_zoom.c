#include "fft/battle.h"
#include "fft/script_variables.h"
#include "fft/world.h"
#include "psx/types.h"

/* Event words 0x1d/0x20 hold the world-map camera tilt and zoom; see
 * EVENT_SCRIPT_VAR_CAMERA_YAW (0x1e) for the neighbouring word. */
enum { WORLD_EVENT_WORD_CAMERA_TILT = 0x1d, WORLD_EVENT_WORD_CAMERA_ZOOM = 0x20 };

/* Steps the camera tilt in units of 0x10 per frame to the nearest of the two
 * resting angles (0x12e below the midpoint, 0x1c0 above it), then reports the
 * band it settled in and restores the default zoom. */
void world_camera_init_tilt_and_zoom(void) {
    s32 tilt;
    s32 target;
    s32 step;
    s32 band;

    tilt = g_world_script_variables[WORLD_EVENT_WORD_CAMERA_TILT];
    band = 0;
    if (tilt < 0x12E) {
        step = 0x10;
        target = 0x12E;
    } else if (tilt < 0x177) {
        step = -0x10;
        target = 0x12E;
    } else if (tilt < 0x1C0) {
        band = 1;
        step = 0x10;
        target = 0x1C0;
    } else {
        band = 1;
        step = -0x10;
        target = 0x1C0;
    }
    if (step < 0) {
        while (target < tilt) {
            g_world_script_variables[WORLD_EVENT_WORD_CAMERA_TILT] = tilt;
            world_thread_yield();
            tilt += step;
        }
        g_world_script_variables[WORLD_EVENT_WORD_CAMERA_TILT] = target;
    } else {
        while (tilt < target) {
            g_world_script_variables[WORLD_EVENT_WORD_CAMERA_TILT] = tilt;
            world_thread_yield();
            tilt += step;
        }
        g_world_script_variables[WORLD_EVENT_WORD_CAMERA_TILT] = target;
    }
    g_battle_map_tilt_target = band + 1;
    g_battle_map_zoom_target = 1;
    g_world_script_variables[WORLD_EVENT_WORD_CAMERA_ZOOM] = ONE;
}
