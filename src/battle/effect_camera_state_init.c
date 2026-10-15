/* Effect camera rotation/position/zoom seeded from the battle camera and the idle track modes. */

#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "psx/types.h"

void effect_copy_camera_angles_and_world_coords(void) {
    SVECTOR* rotation;
    VECTOR* coords;

    rotation = camera_get_rotation();
    g_effect_camera_rotation_saved.vx = g_effect_camera_rotation_start.vx = g_effect_camera_rotation_target.vx
        = g_effect_camera_rotation_current.vx = rotation->vx & 0xfff;
    g_effect_camera_rotation_saved.vy = g_effect_camera_rotation_start.vy = g_effect_camera_rotation_target.vy
        = g_effect_camera_rotation_current.vy = rotation->vy & 0xfff;
    g_effect_camera_rotation_saved.vz = g_effect_camera_rotation_start.vz = g_effect_camera_rotation_target.vz
        = g_effect_camera_rotation_current.vz = rotation->vz & 0xfff;
    coords = camera_get_current_real_coords();
    g_effect_camera_position_saved.vx = g_effect_camera_position_start.vx = g_effect_camera_position_target.vx
        = g_effect_camera_position_current.vx = coords->vx >> 12;
    g_effect_camera_position_saved.vy = g_effect_camera_position_start.vy = g_effect_camera_position_target.vy
        = g_effect_camera_position_current.vy = coords->vy >> 12;
    g_effect_camera_position_saved.vz = g_effect_camera_position_start.vz = g_effect_camera_position_target.vz
        = g_effect_camera_position_current.vz = coords->vz >> 12;
    g_effect_camera_zoom_saved = g_effect_camera_zoom_start = g_effect_camera_zoom_target = g_effect_camera_zoom_current
        = *camera_get_zoom();
}

void effect_init_camera_and_screen_data(void) {
    s32 pad[2];
    g_effect_camera_rotation_mode = EFFECT_CAMERA_MODE_IDLE;
    g_effect_camera_position_mode = EFFECT_CAMERA_MODE_IDLE;
    g_effect_camera_zoom_mode = EFFECT_CAMERA_MODE_IDLE;
    g_battle_unknown_801b8b1c = 0;
    g_battle_unknown_801b8b18 = 0;
    effect_copy_camera_angles_and_world_coords();
    (void)pad;
}
