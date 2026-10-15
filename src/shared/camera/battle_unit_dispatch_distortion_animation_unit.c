#include "fft/battle_camera.h"
#include "fft/battle_unit.h"

/* Step the unit's distortion animation, if it has one: a motion that a
 * sequence started with BATTLE_SEQ_OP_START_DISTORTION and that its handler
 * ends by clearing the id. */
void battle_unit_dispatch_distortion_animation(battle_unit_t* unit) {
    u32 distortion_id = unit->distortion_animation_id;

    if (distortion_id != BATTLE_DISTORTION_ANIMATION_NONE) {
        if (distortion_id < BATTLE_DISTORTION_ANIMATION_COUNT) {
            g_battle_unit_distortion_animation_handlers[distortion_id](unit);
        } else {
            main_system_handle_error(MAIN_SYSTEM_ERROR_DISTORTION_ANIMATION);
        }
    }
}

/* Saves the camera position for camera_restore_current_real_coords (effect
 * script opcodes 09 and 0a). */
void camera_save_current_real_coords(void) {
    copyVector(&g_camera_saved_real_coords, &g_camera_current_real_coords);
}

void camera_restore_current_real_coords(void) {
    copyVector(&g_camera_current_real_coords, &g_camera_saved_real_coords);
}

VECTOR* camera_get_current_real_coords(void) {
    return &g_camera_current_real_coords;
}
