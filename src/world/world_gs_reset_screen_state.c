#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/gte.h"
#include "psx/types.h"

/* libgs screen-state reset: records the screen size, resets the identity,
 * world-screen and light matrices, clears the draw offsets, derives the
 * aspect ratio, sets the clip rectangle and primes the clear tiles. */
void world_gs_reset_screen_state(u16 width, u16 height) {
    g_world_gs_screen_height = height;
    g_world_gs_screen_width = width;
    g_world_gs_id_matrix.m[0][2] = 0;
    g_world_gs_id_matrix.m[0][1] = 0;
    g_world_gs_id_matrix.m[1][2] = 0;
    g_world_gs_id_matrix.m[1][0] = 0;
    g_world_gs_id_matrix.m[2][1] = 0;
    g_world_gs_id_matrix.m[2][0] = 0;
    g_world_gs_id_matrix.t[2] = 0;
    g_world_gs_id_matrix.t[1] = 0;
    g_world_gs_id_matrix.t[0] = 0;
    g_world_gs_id_matrix.m[0][0] = ONE;
    g_world_gs_id_matrix.m[1][1] = ONE;
    g_world_gs_id_matrix.m[2][2] = ONE;
    g_world_gs_ws_matrix = g_world_gs_id_matrix;
    g_world_gs_ls_matrix = g_world_gs_id_matrix;
    g_world_gs_ls_matrix.m[2][2] = 0;
    g_world_gs_ls_matrix.m[1][1] = 0;
    g_world_gs_ls_matrix.m[0][0] = 0;
    g_world_gs_light_ws_matrix = g_world_gs_ls_matrix;
    g_world_gs_buffer_offset_x[0] = 0;
    g_world_gs_buffer_offset_x[1] = 0;
    g_world_gs_buffer_offset_y[0] = 0;
    g_world_gs_buffer_offset_y[1] = 0;
    g_world_gs_ofs.vy = 0;
    g_world_gs_ofs.vx = 0;
    g_world_gs_clip_rect.y = 0;
    g_world_gs_ws_matrix.m[1][1] = (g_world_gs_screen_height << 14) / g_world_gs_screen_width / 3;
    g_world_gs_clip_rect.x = 0;
    g_world_gs_clip_rect.w = g_world_gs_screen_width;
    g_world_gs_clip_rect.h = g_world_gs_screen_height;
    SetBlockFill(&g_world_gs_clear_tile[0]);
    SetBlockFill(&g_world_gs_clear_tile[1]);
    g_world_gs_frame_count = 1;
}
