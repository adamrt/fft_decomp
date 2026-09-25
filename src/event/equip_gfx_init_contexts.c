#include "fft/equip.h"

#define EQUIP_GFX_CONTEXT_COUNT 2

/* Partition each primitive pool across the two graphics contexts.
 *
 * Each pointer uses its pool's halfword capacity and the context index. */
void equip_gfx_init_contexts(equip_gfx_buffer_t* base, u32* otag, POLY_F3* poly_f3, u8* poly_ft3, POLY_F4* poly_f4,
    POLY_FT4* textured_quads, u8* poly_g3, u8* poly_gt3, POLY_G4* poly_g4, POLY_GT4* poly_gt4, LINE_F2* line_f2,
    u8* line_f3, u8* line_f4, LINE_G2* line_g2, u8* line_g3, u8* line_g4, TILE* tiles, u8* tiles_16, u8* tiles_8,
    u8* tiles_1, SPRT* sprites, u8* sprites_16, u8* sprites_8, DR_MOVE* draw_moves, DR_AREA* draw_areas,
    DR_MODE* draw_modes) {
    s32 index;
    equip_gfx_context_t* context;

    g_equip_gfx_context_base = base;
    g_equip_gfx_context = &base->context;

    for (index = 0; index < EQUIP_GFX_CONTEXT_COUNT; index++) {
        context = &g_equip_gfx_context_base[index].context;

        context->otag = otag + g_equip_gfx_otag_length * index;
        context->poly_f3 = poly_f3 + g_equip_gfx_poly_f3_capacity * index;
        context->poly_ft3 = poly_ft3 + g_equip_gfx_poly_ft3_capacity * index * 0x20;
        context->poly_f4 = poly_f4 + g_equip_gfx_poly_f4_capacity * index;
        context->textured_quads = textured_quads + g_equip_gfx_poly_ft4_capacity * index;
        context->poly_g3 = poly_g3 + g_equip_gfx_poly_g3_capacity * index * 0x1C;
        context->poly_gt3 = poly_gt3 + g_equip_gfx_poly_gt3_capacity * index * 0x28;
        context->poly_g4 = poly_g4 + g_equip_gfx_poly_g4_capacity * index;
        context->poly_gt4 = poly_gt4 + g_equip_gfx_poly_gt4_capacity * index;
        context->lines = line_f2 + g_equip_gfx_line_f2_capacity * index;
        context->line_f3 = line_f3 + g_equip_gfx_line_f3_capacity * index * 0x18;
        context->line_f4 = line_f4 + g_equip_gfx_line_f4_capacity * index * 0x1C;
        context->line_g2 = line_g2 + g_equip_gfx_line_g2_capacity * index;
        context->line_g3 = line_g3 + g_equip_gfx_line_g3_capacity * index * 0x20;
        context->line_g4 = line_g4 + g_equip_gfx_line_g4_capacity * index * 0x28;
        context->tiles = tiles + g_equip_gfx_tile_capacity * index;
        context->tiles_16 = tiles_16 + g_equip_gfx_tile_16_capacity * index * 0xC;
        context->tiles_8 = tiles_8 + g_equip_gfx_tile_8_capacity * index * 0xC;
        context->tiles_1 = tiles_1 + g_equip_gfx_tile_1_capacity * index * 0xC;
        context->sprites = sprites + g_equip_gfx_sprite_capacity * index;
        context->sprites_16 = sprites_16 + g_equip_gfx_sprite_16_capacity * index * 0x10;
        context->sprites_8 = sprites_8 + g_equip_gfx_sprite_8_capacity * index * 0x10;
        context->moves = draw_moves + g_equip_gfx_draw_move_capacity * index;
        context->draw_areas = draw_areas + g_equip_gfx_draw_area_capacity * index;
        context->draw_modes = draw_modes + g_equip_gfx_draw_mode_capacity * index;

        equip_gfx_init_primitive_buffers(context);
    }

    equip_gfx_swap_context_and_clear_otag(0, -1);
    equip_gfx_swap_context_and_clear_otag(0, -1);
}
