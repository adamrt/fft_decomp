/* WORLD's double-buffered menu rendering. Each frame submits the completed
 * ordering table, switches draw/display environments and resets the primitive
 * pools for the next frame. Setup binds both buffers to their packet storage. */

#include "fft/gfx_primitive.h"
#include "fft/menu_gfx.h"
#include "fft/menu_screen.h"
#include "fft/system.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"

/*
 * Present the completed ordering table and prepare the other packet buffer.
 *
 * Resets the primitive pool counters, waits for the GPU and the frame step's
 * VSyncs, then flips to the other packet buffer (applying its display and
 * draw environments), submits the finished ordering table from entry
 * draw_otag (-1 skips submission) and clears the new buffer's table from
 * first_otag onward.
 */
void menu_gfx_present_frame(s32 first_otag, s32 draw_otag) {
    DRAWENV env;
    DRAWENV* envp;
    menu_gfx_context_t* context;
    u32* otag = g_menu_gfx_context->otag;

    system_frame_stub();
    envp = &env;
    /* Keep the stack address available before resetting the packet counters;
     * otherwise GCC moves its calculation into GetDrawEnv's delay slot. */
    __asm__("" : : "r"(envp));
    g_menu_gfx_poly_ft4_count = 0;
    g_gfx_poly_g4_count = 0;
    g_menu_gfx_poly_gt4_count = 0;
    g_gfx_line_f2_count = 0;
    g_menu_screen_gradient_line_count = 0;
    g_gfx_tile_count = 0;
    g_menu_gfx_draw_move_count = 0;
    g_menu_screen_draw_area_count = 0;
    g_menu_gfx_draw_mode_count = 0;
    GetDrawEnv(envp);
    g_menu_screen_draw_buffer_clip_y = envp->clip.y;
    while (DrawSync(1) != 0) { }
    VSync(g_menu_event_speed);
    context = g_menu_screen_packet_buffers;
    if (g_menu_gfx_context == context) {
        context++;
    }
    g_menu_gfx_context = context;
    PutDispEnv(&context->disp_env);
    PutDrawEnv(&g_menu_gfx_context->draw_env);
    if (draw_otag != -1) {
        DrawOTag(&otag[draw_otag]);
    }
    ClearOTag(&g_menu_gfx_context->otag[first_otag], g_menu_screen_ot_length - first_otag);
}

/* Initializes every primitive pool of a packet buffer with its libgpu
 * constructor; quads additionally get texture shading enabled. */
static void init_packet_pools(menu_gfx_context_t* buffer) {
    s32 i;

    for (i = 0; i < g_menu_screen_textured_quad_capacity; i++) {
        SetPolyFT4(&buffer->textured_quads[i]);
        SetShadeTex(&buffer->textured_quads[i], 0);
    }
    for (i = 0; i < g_menu_screen_gradient_quad_capacity; i++) {
        SetPolyG4(&buffer->gouraud_quads[i]);
        SetShadeTex(&buffer->gouraud_quads[i], 0);
    }
    for (i = 0; i < g_menu_screen_textured_gradient_quad_capacity; i++) {
        SetPolyGT4(&buffer->gouraud_textured_quads[i]);
        SetShadeTex(&buffer->gouraud_textured_quads[i], 0);
    }
    for (i = 0; i < g_menu_screen_flat_line_capacity; i++) {
        SetLineF2(&buffer->flat_lines[i]);
    }
    for (i = 0; i < g_menu_screen_gradient_line_capacity; i++) {
        SetLineG2(&buffer->gouraud_lines[i]);
    }
    for (i = 0; i < g_gfx_tile_capacity; i++) {
        SetTile(&buffer->tiles[i]);
    }
    for (i = 0; i < g_menu_screen_draw_move_capacity; i++) {
        SetDrawMove(&buffer->draw_moves[i]);
    }
}

/* Binds the primitive pools for both WORLD packet buffers, initializes their
 * draw/display environments, and presents twice to establish the active pair.
 */
void menu_gfx_init_contexts(menu_gfx_context_t* buffers, u32* otags, POLY_F3* unused_2, POLY_FT3* unused_3,
    POLY_F4* unused_4, POLY_FT4* textured_quads, POLY_G3* unused_6, POLY_GT3* unused_7, POLY_G4* gradient_quads,
    POLY_GT4* textured_gradient_quads, LINE_F2* flat_lines, LINE_F3* unused_11, LINE_F4* unused_12,
    LINE_G2* gradient_lines, LINE_G3* unused_14, LINE_G4* unused_15, TILE* tiles, TILE_1* unused_17, TILE_8* unused_18,
    TILE_16* unused_19, SPRT* unused_20, SPRT_8* unused_21, SPRT_16* unused_22, DR_MOVE* draw_moves,
    DR_AREA* draw_areas, DR_MODE* draw_modes) {
    s32 i;

    g_menu_screen_packet_buffers = buffers;
    g_menu_gfx_context = buffers;
    for (i = 0; i < 2; i++) {
        g_menu_screen_packet_buffers[i].otag = otags + g_menu_screen_ot_length * i;
        g_menu_screen_packet_buffers[i].textured_quads = textured_quads + g_menu_screen_textured_quad_capacity * i;
        g_menu_screen_packet_buffers[i].gouraud_quads = gradient_quads + g_menu_screen_gradient_quad_capacity * i;
        g_menu_screen_packet_buffers[i].gouraud_textured_quads
            = textured_gradient_quads + g_menu_screen_textured_gradient_quad_capacity * i;
        g_menu_screen_packet_buffers[i].flat_lines = flat_lines + g_menu_screen_flat_line_capacity * i;
        g_menu_screen_packet_buffers[i].gouraud_lines = gradient_lines + g_menu_screen_gradient_line_capacity * i;
        g_menu_screen_packet_buffers[i].tiles = tiles + g_gfx_tile_capacity * i;
        g_menu_screen_packet_buffers[i].draw_moves = draw_moves + g_menu_screen_draw_move_capacity * i;
        g_menu_screen_packet_buffers[i].draw_areas = draw_areas + g_menu_screen_draw_area_capacity * i;
        g_menu_screen_packet_buffers[i].draw_modes = draw_modes + g_gfx_draw_mode_capacity * i;
        init_packet_pools(&g_menu_screen_packet_buffers[i]);
        SetDefDrawEnv(&g_menu_screen_packet_buffers[i].draw_env, 0, i * 240, 255, 240);
        SetDefDispEnv(&g_menu_screen_packet_buffers[i].disp_env, 0, i == 0 ? 240 : 0, 255, 240);
        g_menu_screen_packet_buffers[i].draw_env.isbg = 0;
        g_menu_screen_packet_buffers[i].draw_env.dfe = 1;
        setRGB0(&g_menu_screen_packet_buffers[i].draw_env, 0, 0, 0);
        g_menu_screen_packet_buffers[i].draw_env.ofs[0] = -128;
    }
    menu_gfx_present_frame(0, -1);
    menu_gfx_present_frame(0, -1);
}
