#include "fft/event_jobstts.h"
#include "psx/gpu.h"
#include "psx/types.h"

#define JOBSTTS_GFX_CONTEXT_COUNT 2

/* Partition each primitive pool across the two graphics contexts.
 *
 * JOBSTTS twin of equip_gfx_init_contexts: each pointer uses its pool's
 * halfword capacity and the context index. The capacities are read as
 * unsigned halfwords; the LINE_G2/LINE_G3 ones are volatile so their loads
 * stay in field order. */
void jobstts_gfx_init_contexts(jobstts_gfx_context_t* base, u32* otag, POLY_F3* poly_f3, u8* poly_ft3, POLY_F4* poly_f4,
    POLY_FT4* textured_quads, u8* poly_g3, u8* poly_gt3, POLY_G4* poly_g4, POLY_GT4* poly_gt4, LINE_F2* line_f2,
    u8* line_f3, u8* line_f4, LINE_G2* line_g2, u8* line_g3, u8* line_g4, TILE* tiles, u8* tiles_16, u8* tiles_8,
    u8* tiles_1, SPRT* sprites, u8* sprites_16, u8* sprites_8, DR_MOVE* draw_moves, DR_AREA* draw_areas) {
    s32 index;
    jobstts_gfx_context_t* context;

    g_jobstts_gfx_context_base = base;
    g_jobstts_gfx_context = base;

    for (index = 0; index < JOBSTTS_GFX_CONTEXT_COUNT; index++) {
        context = &g_jobstts_gfx_context_base[index];

        context->otag = otag + g_jobstts_gfx_otag_length * index;
        context->poly_f3 = poly_f3 + (u16)g_jobstts_gfx_poly_f3_capacity * index;
        context->poly_ft3 = poly_ft3 + (u16)g_jobstts_gfx_poly_ft3_capacity * index * 0x20;
        context->poly_f4 = poly_f4 + (u16)g_jobstts_gfx_poly_f4_capacity * index;
        context->textured_quads = textured_quads + g_jobstts_gfx_poly_ft4_capacity * index;
        context->poly_g3 = poly_g3 + (u16)g_jobstts_gfx_poly_g3_capacity * index * 0x1C;
        context->poly_gt3 = poly_gt3 + (u16)g_jobstts_gfx_poly_gt3_capacity * index * 0x28;
        context->poly_g4 = poly_g4 + (u16)g_jobstts_gfx_poly_g4_capacity * index;
        context->poly_gt4 = poly_gt4 + (u16)g_jobstts_gfx_poly_gt4_capacity * index;
        context->line_f2 = line_f2 + (u16)g_jobstts_gfx_line_f2_capacity * index;
        context->line_f3 = line_f3 + (u16)g_jobstts_gfx_line_f3_capacity * index * 0x18;
        context->line_f4 = line_f4 + (u16)g_jobstts_gfx_line_f4_capacity * index * 0x1C;
        context->line_g2 = line_g2 + g_jobstts_gfx_line_g2_capacity * index;
        context->line_g3 = line_g3 + g_jobstts_gfx_line_g3_capacity * index * 0x20;
        context->line_g4 = line_g4 + (u16)g_jobstts_gfx_line_g4_capacity * index * 0x28;
        context->tiles = tiles + g_jobstts_gfx_tile_capacity * index;
        context->tiles_16 = tiles_16 + (u16)g_jobstts_gfx_tile_16_capacity * index * 0xC;
        context->tiles_8 = tiles_8 + (u16)g_jobstts_gfx_tile_8_capacity * index * 0xC;
        context->tiles_1 = tiles_1 + (u16)g_jobstts_gfx_tile_1_capacity * index * 0xC;
        context->sprites = sprites + (u16)g_jobstts_gfx_sprite_capacity * index;
        context->sprites_16 = sprites_16 + (u16)g_jobstts_gfx_sprite_16_capacity * index * 0x10;
        context->sprites_8 = sprites_8 + (u16)g_jobstts_gfx_sprite_8_capacity * index * 0x10;
        context->draw_moves = draw_moves + g_jobstts_gfx_draw_move_capacity * index;
        context->draw_areas = draw_areas + g_jobstts_gfx_draw_area_capacity * index;

        jobstts_gfx_init_primitive_buffers(context);
    }

    /* The target passes a second argument the callee does not take. */
    ((void (*)(s32, s32))jobstts_gfx_swap_context_and_clear_otag)(0, -1);
    ((void (*)(s32, s32))jobstts_gfx_swap_context_and_clear_otag)(0, -1);
}
