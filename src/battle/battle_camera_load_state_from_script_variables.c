#include "fft/battle.h"
#include "psx/types.h"

/* Load camera rotation, zoom and position back out of the script variable
 * block; the inverse of battle_camera_store_state_to_script_variables. */
void battle_camera_load_state_from_script_variables(void) {
    g_battle_camera_script_rotation.x = g_battle_script_variables[BATTLE_CAMERA_TILT_WORD_INDEX];
    g_battle_camera_script_rotation.y = g_battle_script_variables[BATTLE_CAMERA_YAW_WORD_INDEX];
    g_battle_camera_script_rotation.z = g_battle_script_variables[BATTLE_CAMERA_ROLL_WORD_INDEX];
    battle_camera_set_rotation(&g_battle_camera_script_rotation);

    g_battle_camera_script_vector.vx = g_battle_script_variables[BATTLE_CAMERA_ZOOM_WORD_INDEX];
    g_battle_camera_script_vector.vy = g_battle_script_variables[BATTLE_CAMERA_ZOOM_WORD_INDEX];
    g_battle_camera_script_vector.vz = g_battle_script_variables[BATTLE_CAMERA_ZOOM_WORD_INDEX];
    battle_camera_set_zoom(&g_battle_camera_script_vector);

    g_battle_camera_script_vector.vx = g_battle_script_variables[BATTLE_CAMERA_X_WORD_INDEX];
    g_battle_camera_script_vector.vy
        = (g_battle_camera_shake_z_offset << 12) + g_battle_script_variables[BATTLE_CAMERA_Z_WORD_INDEX];
    g_battle_camera_script_vector.vz = g_battle_script_variables[BATTLE_CAMERA_Y_WORD_INDEX];
    battle_camera_set_current_real_coords(&g_battle_camera_script_vector);
}
