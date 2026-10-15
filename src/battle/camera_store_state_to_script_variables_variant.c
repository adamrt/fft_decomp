#include "fft/battle_camera.h"
#include "fft/battle_script.h"
#include "fft/script.h"

/* Publish the live camera rotation, zoom, and position to the script variable
 * block.
 *
 * The three zoom components all land in the same 32-bit script variable; that is what the
 * target does. */
void camera_store_state_to_script_variables_variant(void) {
    SVECTOR* rotation;
    VECTOR* zoom;
    VECTOR* coords;

    rotation = camera_get_rotation();
    g_script_variables[EVENT_SCRIPT_VAR_CAMERA_TILT] = rotation->vx;
    g_script_variables[EVENT_SCRIPT_VAR_CAMERA_YAW] = rotation->vy;
    g_camera_published_rotation_ptr = rotation;
    g_script_variables[EVENT_SCRIPT_VAR_CAMERA_ROLL] = rotation->vz;

    zoom = camera_get_zoom();
    g_script_variables[EVENT_SCRIPT_VAR_CAMERA_ZOOM] = zoom->vx;
    g_script_variables[EVENT_SCRIPT_VAR_CAMERA_ZOOM] = zoom->vy;
    g_camera_published_coords_ptr = zoom;
    g_script_variables[EVENT_SCRIPT_VAR_CAMERA_ZOOM] = zoom->vz;

    coords = camera_get_current_real_coords();
    g_script_variables[EVENT_SCRIPT_VAR_CAMERA_X] = coords->vx;
    g_script_variables[EVENT_SCRIPT_VAR_CAMERA_Z] = coords->vy - (g_camera_shake_z_offset * ONE);
    g_camera_published_coords_ptr = coords;
    g_camera_shake_z_offset = 0;
    g_script_variables[EVENT_SCRIPT_VAR_CAMERA_Y] = coords->vz;
}
