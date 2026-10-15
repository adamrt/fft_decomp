#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Binds the primitive pools for both WORLD packet buffers, initializes their
 * draw/display environments, and presents twice to establish the active pair.
 */
void world_gfx_init_double_packet_buffers(menu_gfx_context_t* buffers, u32* otags, void* unused_2, void* unused_3,
    void* unused_4, POLY_FT4* textured_quads, void* unused_6, void* unused_7, POLY_G4* gradient_quads,
    POLY_GT4* textured_gradient_quads, LINE_F2* flat_lines, void* unused_11, void* unused_12, LINE_G2* gradient_lines,
    void* unused_14, void* unused_15, TILE* tiles, void* unused_17, void* unused_18, void* unused_19, void* unused_20,
    void* unused_21, void* unused_22, DR_MOVE* draw_moves, DR_AREA* draw_areas, DR_MODE* draw_modes) {
    s32 i;

    g_world_gfx_packet_buffers = buffers;
    g_menu_gfx_context = buffers;
    for (i = 0; i < 2; i++) {
        g_world_gfx_packet_buffers[i].otag = otags + g_world_ot_length * i;
        g_world_gfx_packet_buffers[i].textured_quads = textured_quads + g_world_gfx_textured_quad_capacity * i;
        g_world_gfx_packet_buffers[i].gouraud_quads = gradient_quads + g_world_gfx_gradient_quad_capacity * i;
        g_world_gfx_packet_buffers[i].gouraud_textured_quads
            = textured_gradient_quads + g_world_gfx_textured_gradient_quad_capacity * i;
        g_world_gfx_packet_buffers[i].flat_lines = flat_lines + g_world_gfx_flat_line_capacity * i;
        g_world_gfx_packet_buffers[i].gouraud_lines = gradient_lines + g_world_gfx_gradient_line_capacity * i;
        g_world_gfx_packet_buffers[i].tiles = tiles + g_gfx_tile_capacity * i;
        g_world_gfx_packet_buffers[i].draw_moves = draw_moves + g_world_gfx_draw_move_capacity * i;
        g_world_gfx_packet_buffers[i].draw_areas = draw_areas + g_world_gfx_draw_area_capacity * i;
        g_world_gfx_packet_buffers[i].draw_modes = draw_modes + g_gfx_draw_mode_capacity * i;
        world_gfx_init_packet_pools(&g_world_gfx_packet_buffers[i]);
        SetDefDrawEnv(&g_world_gfx_packet_buffers[i].draw_env, 0, i * 240, 255, 240);
        SetDefDispEnv(&g_world_gfx_packet_buffers[i].disp_env, 0, i == 0 ? 240 : 0, 255, 240);
        g_world_gfx_packet_buffers[i].draw_env.isbg = 0;
        g_world_gfx_packet_buffers[i].draw_env.dfe = 1;
        setRGB0(&g_world_gfx_packet_buffers[i].draw_env, 0, 0, 0);
        g_world_gfx_packet_buffers[i].draw_env.ofs[0] = -128;
    }
    world_gfx_present_frame_and_swap_packet_buffer(0, -1);
    world_gfx_present_frame_and_swap_packet_buffer(0, -1);
}
