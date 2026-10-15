#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "psx/types.h"

/* Projects a world position to screen x and y. Rebuilds the camera matrices first
 * and leaves them loaded in the GTE. */
void camera_project_world_coords_to_screen(SVECTOR* world_coords, SVECTOR* screen_coords) {
    VECTOR transformed_coords;
    s32 result;

    camera_update_matrices(&g_camera_matrix, &g_camera_render_state, &g_camera_current_screen_offset, &g_camera_zoom);
    SetRotMatrix(&g_camera_matrix);
    SetTransMatrix(&g_camera_matrix);
    RotTrans(world_coords, &transformed_coords, &result);
    screen_coords->vx = transformed_coords.vx;
    screen_coords->vy = transformed_coords.vy;
}
