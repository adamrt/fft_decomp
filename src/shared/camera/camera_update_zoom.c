#include "fft/battle_camera.h"
#include "fft/battle_map.h"
#include "psx/types.h"

/* Step the three zoom scales toward the zoom target, ONE zooming in or 0xc00
 * zooming out, by an increment that grows by four times the animation speed
 * each frame until it reaches 0x300. The zoom ends on the target. */
void camera_update_zoom(void) {
    s32 increment;
    s32* zoom_ptr;

    if (g_camera_zoom_action == CAMERA_STEP_INCREASE && g_map_zoom_target == MAP_ZOOM_IN) {
        s32 zoom;
        s32 below;
        s32 pan_x;
        s32 pan_y;

        zoom_ptr = &g_camera_zoom.vx;
        zoom = *zoom_ptr;
        increment = g_camera_zoom_increment;
        zoom = zoom + increment;
        *zoom_ptr = zoom;
        below = zoom < ONE;
        pan_x = g_camera_zoom.vy;
        pan_y = g_camera_zoom.vz;
        pan_x = pan_x + increment;
        pan_y = pan_y + increment;
        g_camera_zoom.vy = pan_x;
        g_camera_zoom.vz = pan_y;
        if (!below) {
            g_camera_zoom_action = CAMERA_STEP_NONE;
            g_camera_zoom.vz = ONE;
            g_camera_zoom.vy = ONE;
            *zoom_ptr = ONE;
        }
    } else if (g_camera_zoom_action == CAMERA_STEP_DECREASE && g_map_zoom_target == g_camera_zoom_action) {
        s32 zoom;
        s32 below;

        zoom_ptr = &g_camera_zoom.vx;
        zoom = *zoom_ptr;
        increment = g_camera_zoom_increment;
        zoom = zoom - increment;
        *zoom_ptr = zoom;
        below = zoom < 0xC01;
        g_camera_zoom.vy -= increment;
        g_camera_zoom.vz -= increment;
        if (below) {
            g_camera_zoom_action = CAMERA_STEP_NONE;
            g_camera_zoom.vz = 0xC00;
            g_camera_zoom.vy = 0xC00;
            *zoom_ptr = 0xC00;
        }
    } else {
        return;
    }
    if (increment < 0x300) {
        g_camera_zoom_increment = g_main_system_animation_speed * 4 + increment;
    }
}
