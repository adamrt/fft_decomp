#include "fft/event_card.h"
#include "psx/libgpu.h"
#include "psx/types.h"

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
