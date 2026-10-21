/* Swap EQUIP's drawing contexts and partition and initialize their
 * primitive pools for the equipment screen frame. */

#include "fft/equipment.h"
#include "fft/gfx.h"
#include "fft/gfx_primitive.h"
#include "fft/menu_gfx.h"
#include "psx/libgpu.h"
#include "psx/types.h"

static menu_gfx_context_t* g_equipment_gfx_context_base;
extern u16 g_equipment_gfx_poly_f4_count;
extern u16 g_equipment_gfx_poly_g4_count;

/* Reset primitive counts, swap packet buffers, and clear the active OT tail. */
void equipment_gfx_swap_context(s32 first_otag, s32 unused) {
    DRAWENV environment;
    menu_gfx_context_t* buffer;

    g_gfx_poly_f3_count = 0;
    g_gfx_poly_ft3_count = 0;
    g_equipment_gfx_poly_f4_count = 0;
    g_menu_gfx_poly_ft4_count = 0;
    g_gfx_poly_g3_count = 0;
    g_gfx_poly_gt3_count = 0;
    g_equipment_gfx_poly_g4_count = 0;
    g_gfx_poly_gt4_count = 0;
    g_gfx_line_f2_count = 0;
    g_gfx_line_f3_count = 0;
    g_gfx_line_f4_count = 0;
    g_gfx_line_g2_count = 0;
    g_gfx_line_g3_count = 0;
    g_gfx_line_g4_count = 0;
    g_gfx_tile_count = 0;
    g_gfx_tile_1_count = 0;
    g_gfx_tile_8_count = 0;
    g_gfx_tile_16_count = 0;
    g_gfx_sprite_count = 0;
    g_gfx_sprite_8_count = 0;
    g_gfx_sprite_16_count = 0;
    g_menu_gfx_draw_move_count = 0;
    g_menu_gfx_draw_area_count = 0;
    g_menu_gfx_draw_mode_count = 0;
    GetDrawEnv(&environment);
    buffer = g_equipment_gfx_context_base;
    g_menu_gfx_drawenv_y = environment.clip.y;
    if (g_menu_gfx_context == buffer) {
        buffer++;
    }
    g_menu_gfx_context = buffer;
    ClearOTag(&buffer->otag[first_otag], g_gfx_otag_length - first_otag);
}

/* Initialize the LINE_F2, POLY_F4, POLY_FT4, TILE and DR_MOVE primitive
 * headers of one graphics context, up to each pool's capacity; the quads
 * also get texture shading enabled. */
static void equipment_gfx_init_primitive_buffers(menu_gfx_context_t* context) {
    s32 i;

    for (i = 0; i < g_gfx_line_f2_capacity; i++) {
        SetLineF2(&context->flat_lines[i]);
    }
    for (i = 0; i < g_gfx_poly_f4_capacity; i++) {
        SetPolyF4(&context->flat_quads[i]);
        SetShadeTex(&context->flat_quads[i], 0);
    }
    for (i = 0; i < g_gfx_poly_ft4_capacity; i++) {
        SetPolyFT4(&context->textured_quads[i]);
        SetShadeTex(&context->textured_quads[i], 0);
    }
    for (i = 0; i < g_gfx_tile_capacity; i++) {
        SetTile(&context->tiles[i]);
    }
    for (i = 0; i < g_equipment_gfx_draw_move_capacity; i++) {
        SetDrawMove(&context->draw_moves[i]);
    }
}

#define EQUIPMENT_GFX_CONTEXT_COUNT 2

/* Partition each primitive pool across the two graphics contexts.
 *
 * Each pointer uses its pool's 16-bit capacity and the context index. */
void equipment_gfx_init_contexts(menu_gfx_context_t* base, u32* otag, POLY_F3* poly_f3, POLY_FT3* poly_ft3,
    POLY_F4* poly_f4, POLY_FT4* textured_quads, POLY_G3* poly_g3, POLY_GT3* poly_gt3, POLY_G4* poly_g4,
    POLY_GT4* poly_gt4, LINE_F2* line_f2, LINE_F3* line_f3, LINE_F4* line_f4, LINE_G2* line_g2, LINE_G3* line_g3,
    LINE_G4* line_g4, TILE* tiles, TILE_1* tiles_1, TILE_8* tiles_8, TILE_16* tiles_16, SPRT* sprites,
    SPRT_8* sprites_8, SPRT_16* sprites_16, DR_MOVE* draw_moves, DR_AREA* draw_areas, DR_MODE* draw_modes) {
    s32 index;
    menu_gfx_context_t* context;

    g_equipment_gfx_context_base = base;
    g_menu_gfx_context = base;

    for (index = 0; index < EQUIPMENT_GFX_CONTEXT_COUNT; index++) {
        context = &g_equipment_gfx_context_base[index];

        context->otag = otag + g_gfx_otag_length * index;
        context->flat_triangles = poly_f3 + g_gfx_poly_f3_capacity * index;
        context->textured_triangles = poly_ft3 + g_gfx_poly_ft3_capacity * index;
        context->flat_quads = poly_f4 + g_gfx_poly_f4_capacity * index;
        context->textured_quads = textured_quads + g_gfx_poly_ft4_capacity * index;
        context->gouraud_triangles = poly_g3 + g_gfx_poly_g3_capacity * index;
        context->gouraud_textured_triangles = poly_gt3 + g_gfx_poly_gt3_capacity * index;
        context->gouraud_quads = poly_g4 + g_gfx_poly_g4_capacity * index;
        context->gouraud_textured_quads = poly_gt4 + g_gfx_poly_gt4_capacity * index;
        context->flat_lines = line_f2 + g_gfx_line_f2_capacity * index;
        context->flat_lines_3 = line_f3 + g_gfx_line_f3_capacity * index;
        context->flat_lines_4 = line_f4 + g_gfx_line_f4_capacity * index;
        context->gouraud_lines = line_g2 + g_equipment_gfx_line_g2_capacity * index;
        context->gouraud_lines_3 = line_g3 + g_equipment_gfx_line_g3_capacity * index;
        context->gouraud_lines_4 = line_g4 + g_gfx_line_g4_capacity * index;
        context->tiles = tiles + g_gfx_tile_capacity * index;
        context->tiles_1 = tiles_1 + g_gfx_tile_1_capacity * index;
        context->tiles_8 = tiles_8 + g_gfx_tile_8_capacity * index;
        context->tiles_16 = tiles_16 + g_gfx_tile_16_capacity * index;
        context->sprites = sprites + g_gfx_sprite_capacity * index;
        context->sprites_8 = sprites_8 + g_gfx_sprite_8_capacity * index;
        context->sprites_16 = sprites_16 + g_gfx_sprite_16_capacity * index;
        context->draw_moves = draw_moves + g_equipment_gfx_draw_move_capacity * index;
        context->draw_areas = draw_areas + g_equipment_gfx_draw_area_capacity * index;
        context->draw_modes = draw_modes + g_gfx_draw_mode_capacity * index;

        equipment_gfx_init_primitive_buffers(context);
    }

    equipment_gfx_swap_context(0, -1);
    equipment_gfx_swap_context(0, -1);
}
