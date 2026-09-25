#include "fft/card.h"
#include "psx/gpu.h"
#include "psx/types.h"

void card_gfx_init_contexts(card_graphics_context_t* contexts, u32* otags, s32 unused_2, s32 unused_3, s32 unused_4,
    POLY_FT4* polygons, s32 unused_6, s32 unused_7, s32 unused_8, s32 unused_9, s32 unused_10, s32 unused_11,
    s32 unused_12, s32 unused_13, s32 unused_14, s32 unused_15, TILE* tiles) {
    s32 i;
    s32 display_height;
    s32 y_offset;
    s32 context_offset;
    card_graphics_context_t* setup_context;

    i = 0;
    display_height = 0xf0;
    y_offset = 0;
    context_offset = 0;
    g_card_gfx_context_base = contexts;
    g_card_gfx_context = contexts;
    for (; i < 2; i++) {
        setup_context = (card_graphics_context_t*)((u8*)g_card_gfx_context_base + context_offset);
        setup_context->otag = otags + g_card_gfx_otag_length * i;
        setup_context->polygons = polygons + g_card_gfx_poly_ft4_capacity * i;
        setup_context->tiles = tiles + g_card_gfx_tile_capacity * i;
        card_gfx_init_primitive_lists((card_primitive_lists_t*)setup_context);
        SetDefDrawEnv(&((card_graphics_context_t*)(context_offset + (s32)g_card_gfx_context_base))->draw_environment, 0,
            y_offset, 0x100, display_height);
        SetDefDispEnv(&((card_graphics_context_t*)(context_offset + (s32)g_card_gfx_context_base))->display_environment,
            0, i == 0 ? 0xf0 : 0, 0x100, display_height);
        ((card_graphics_context_t*)(context_offset + (s32)g_card_gfx_context_base))->draw_environment.isbg = 0;
        ((card_graphics_context_t*)(context_offset + (s32)g_card_gfx_context_base))->draw_environment.r0 = 0;
        ((card_graphics_context_t*)(context_offset + (s32)g_card_gfx_context_base))->draw_environment.g0 = 0;
        ((card_graphics_context_t*)(context_offset + (s32)g_card_gfx_context_base))->draw_environment.b0 = 0;
        ((card_graphics_context_t*)(context_offset + (s32)g_card_gfx_context_base))->draw_environment.ofs[0] = -0x80;
        y_offset += 0xf0;
        context_offset += sizeof(card_graphics_context_t);
    }
    card_gfx_submit_frame_and_swap_buffers(0, -1);
    card_gfx_submit_frame_and_swap_buffers(0, -1);
}
