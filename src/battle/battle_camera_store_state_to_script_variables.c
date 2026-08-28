#include "fft/battle.h"
#include "psx/types.h"

/* Publish the live camera rotation, zoom, and position to the script variable
 * block.
 *
 * The three zoom components all land in the same script word; that is what the
 * target does. */
void battle_camera_store_state_to_script_variables(void) {
    SVECTOR* rotation;
    VECTOR* zoom;
    VECTOR* coords;

    rotation = (SVECTOR*)battle_camera_get_rotation();
    g_battle_script_variables[BATTLE_CAMERA_TILT_WORD_INDEX] = rotation->vx;
    g_battle_script_variables[BATTLE_CAMERA_YAW_WORD_INDEX] = rotation->vy;
    g_battle_camera_published_rotation_ptr = rotation;
    g_battle_script_variables[BATTLE_CAMERA_ROLL_WORD_INDEX] = rotation->vz;

    zoom = battle_camera_get_zoom();
    g_battle_script_variables[BATTLE_CAMERA_ZOOM_WORD_INDEX] = zoom->vx;
    g_battle_script_variables[BATTLE_CAMERA_ZOOM_WORD_INDEX] = zoom->vy;
    g_battle_camera_published_coords_ptr = zoom;
    g_battle_script_variables[BATTLE_CAMERA_ZOOM_WORD_INDEX] = zoom->vz;

    coords = battle_camera_get_current_real_coords();
    g_battle_script_variables[BATTLE_CAMERA_X_WORD_INDEX] = coords->vx;
    g_battle_script_variables[BATTLE_CAMERA_Z_WORD_INDEX] = coords->vy - (g_battle_camera_shake_z_offset << 12);
    g_battle_camera_published_coords_ptr = coords;
    g_battle_camera_shake_z_offset = 0;
    g_battle_script_variables[BATTLE_CAMERA_Y_WORD_INDEX] = coords->vz;
}
