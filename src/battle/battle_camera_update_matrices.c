#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "psx/types.h"

void battle_camera_update_matrices(
    MATRIX* camera_matrix, void* render_buffers, void* offset_screen_coords, VECTOR* camera_zoom) {
    u32 result;

    g_battle_map_camera_zoom = *camera_zoom;
    memcpy(&g_battle_map_camera_rotation, render_buffers, 8);

    RotMatrix(render_buffers, camera_matrix);
    TransMatrix(camera_matrix, offset_screen_coords);
    ScaleMatrix(camera_matrix, &g_battle_map_camera_zoom);
    RotMatrix(render_buffers, &g_battle_map_camera_matrix);
    SetRotMatrix(camera_matrix);
    SetTransMatrix(camera_matrix);
    RotTrans(&g_battle_camera_integer_coords, (VECTOR*)g_battle_map_camera_matrix.t, (long*)&result);
    ScaleMatrix(&g_battle_map_camera_matrix, &g_battle_map_camera_zoom);

    *camera_matrix = g_battle_map_camera_matrix;
    g_battle_map_mesh_parts[0].matrix_60 = g_battle_map_camera_matrix;
    RotMatrix(&g_battle_map_light_rotation, &g_battle_map_light_rotation_matrix);
}
