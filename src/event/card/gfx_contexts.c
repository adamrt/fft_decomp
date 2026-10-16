/* Frame submission, primitive lists and graphics context init. */

#include "fft/event_card.h"
#include "fft/gfx.h"
#include "fft/main.h"
#include "fft/menu.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/*
 * Submit the finished CARD frame and swap to the other graphics context.
 *
 * Resets the primitive counts, waits for the GPU and the configured VSync
 * count, installs the other context's display and draw environments, draws
 * the finished ordering table from entry draw_otag (-1 skips the draw) and
 * clears the new one from first_otag onward.
 */
void card_gfx_submit_frame_and_swap_buffers(s32 first_otag, s32 draw_otag) {
    u32* previous_otag = g_card_gfx_context->otag;
    card_graphics_context_t* context;

    main_system_frame_stub();
    g_menu_gfx_poly_ft4_count = 0;
    g_gfx_tile_count = 0;
    while (DrawSync(1) != 0) { }
    VSync(g_menu_event_speed);
    context = g_card_gfx_context_base;
    if (g_card_gfx_context == context) {
        context = context + 1;
    }
    g_card_gfx_context = context;
    PutDispEnv(&context->display_environment);
    PutDrawEnv(&g_card_gfx_context->draw_environment);
    g_card_gfx_drawenv_y = g_card_gfx_context->draw_environment.clip.y;
    if (draw_otag != -1) {
        DrawOTag(previous_otag + draw_otag);
    }
    ClearOTag(g_card_gfx_context->otag + first_otag, g_gfx_otag_count - first_otag);
}

/* Initialize the POLY_FT4 and TILE primitive headers of one graphics context. */
static void card_gfx_init_primitive_lists(card_graphics_context_t* lists) {
    s32 i;

    for (i = 0; i < g_gfx_poly_ft4_capacity; i++) {
        SetPolyFT4(&lists->polygons[i]);
        SetShadeTex(&lists->polygons[i], 0);
    }
    for (i = 0; i < g_gfx_tile_capacity; i++) {
        SetTile(&lists->tiles[i]);
    }
}

/* Partition the ordering tables, the POLY_FT4 pool and the TILE pool across
 * the two graphics contexts and set up their 256x240 draw and display
 * environments (context 0 draws at y 0 and displays y 0xf0, context 1 the
 * reverse).
 *
 * CARD copy of world_gfx_init_double_packet_buffers reduced to the POLY_FT4
 * and TILE pools; the pools this overlay does not use are arguments nobody
 * reads. */
void card_gfx_init_contexts(card_graphics_context_t* contexts, u32* otags, s32 unused_2, s32 unused_3, s32 unused_4,
    POLY_FT4* polygons, s32 unused_6, s32 unused_7, s32 unused_8, s32 unused_9, s32 unused_10, s32 unused_11,
    s32 unused_12, s32 unused_13, s32 unused_14, s32 unused_15, TILE* tiles, s32 unused_17, s32 unused_18,
    s32 unused_19, s32 unused_20, s32 unused_21, s32 unused_22, s32 unused_23, s32 unused_24) {
    s32 i;

    g_card_gfx_context_base = contexts;
    g_card_gfx_context = contexts;
    for (i = 0; i < 2; i++) {
        g_card_gfx_context_base[i].otag = otags + g_gfx_otag_count * i;
        g_card_gfx_context_base[i].polygons = polygons + g_gfx_poly_ft4_capacity * i;
        g_card_gfx_context_base[i].tiles = tiles + g_gfx_tile_capacity * i;
        card_gfx_init_primitive_lists(&g_card_gfx_context_base[i]);
        SetDefDrawEnv(&g_card_gfx_context_base[i].draw_environment, 0, i * 0xf0, 0x100, 0xf0);
        SetDefDispEnv(&g_card_gfx_context_base[i].display_environment, 0, i == 0 ? 0xf0 : 0, 0x100, 0xf0);
        g_card_gfx_context_base[i].draw_environment.isbg = 0;
        setRGB0(&g_card_gfx_context_base[i].draw_environment, 0, 0, 0);
        g_card_gfx_context_base[i].draw_environment.ofs[0] = -0x80;
    }
    card_gfx_submit_frame_and_swap_buffers(0, -1);
    card_gfx_submit_frame_and_swap_buffers(0, -1);
}
