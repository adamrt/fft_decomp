#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/*
 * Present the completed ordering table and prepare the other packet buffer.
 *
 * draw_otag == -1 skips submission. The incoming table is cleared from
 * first_otag onward after waiting for the GPU and the configured VSync count.
 */
void world_gfx_present_frame_and_swap_packet_buffer(s32 first_otag, s32 draw_otag) {
    DRAWENV env;
    DRAWENV* envp;
    world_gfx_packet_buffer_t* context;
    u32* otag = g_world_gfx_active_packet_buffer->otag;

    main_noop_800449ec();
    envp = &env;
    /* Keep the stack address available before resetting the packet counters;
     * otherwise GCC moves its calculation into GetDrawEnv's delay slot. */
    __asm__("" : : "r"(envp));
    g_world_gfx_textured_quad_count = 0;
    g_world_gfx_gradient_quad_count = 0;
    g_world_gfx_textured_gradient_quad_count = 0;
    g_world_gfx_tiles_24_count = 0;
    g_world_gfx_gradient_line_count = 0;
    g_world_gfx_tile_count = 0;
    g_world_gfx_draw_move_count = 0;
    g_world_gfx_draw_area_count = 0;
    g_world_gfx_draw_mode_count = 0;
    GetDrawEnv(envp);
    g_world_gfx_draw_buffer_clip_y = envp->clip.y;
    while (DrawSync(1) != 0) { }
    VSync(g_world_gfx_vsync_mode);
    context = g_world_gfx_packet_buffers;
    if (g_world_gfx_active_packet_buffer == context) {
        context++;
    }
    g_world_gfx_active_packet_buffer = context;
    PutDispEnv(&context->disp_env);
    PutDrawEnv(&g_world_gfx_active_packet_buffer->draw_env);
    if (draw_otag != -1) {
        DrawOTag(&otag[draw_otag]);
    }
    ClearOTag(&g_world_gfx_active_packet_buffer->otag[first_otag], g_world_ot_length - first_otag);
}
