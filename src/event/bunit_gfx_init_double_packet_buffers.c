#include "fft/event_bunit.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Per-buffer pool capacities, one per pointer above, set by
 * bunit_entrypoint before it calls the initializer. Volatile: the target
 * reads (and its caller writes) them strictly in field order, which only
 * volatile accesses keep the scheduler from interleaving. */
extern volatile u16 g_bunit_gfx_poly_f3_capacity;
extern volatile u16 g_bunit_gfx_poly_ft3_capacity;
extern volatile u16 g_bunit_gfx_poly_f4_capacity;
extern volatile u16 g_bunit_gfx_poly_ft4_capacity;
extern volatile u16 g_bunit_gfx_poly_g3_capacity;
extern volatile u16 g_bunit_gfx_poly_gt3_capacity;
extern volatile u16 g_bunit_gfx_poly_g4_capacity;
extern volatile u16 g_bunit_gfx_poly_gt4_capacity;
extern volatile u16 g_bunit_gfx_line_f2_capacity;
extern volatile u16 g_bunit_gfx_line_f3_capacity;
extern volatile u16 g_bunit_gfx_line_f4_capacity;
extern volatile u16 g_bunit_gfx_line_g2_capacity;
extern volatile u16 g_bunit_gfx_line_g3_capacity;
extern volatile u16 g_bunit_gfx_line_g4_capacity;
extern volatile u16 g_bunit_gfx_tile_capacity;
extern volatile u16 g_bunit_gfx_tile_1_capacity;
extern volatile u16 g_bunit_gfx_tile_8_capacity;
extern volatile u16 g_bunit_gfx_tile_16_capacity;
extern volatile u16 g_bunit_gfx_sprite_capacity;
extern volatile u16 g_bunit_gfx_sprite_8_capacity;
extern volatile u16 g_bunit_gfx_sprite_16_capacity;
extern volatile u16 g_bunit_gfx_draw_move_capacity;
extern volatile u16 g_bunit_gfx_draw_area_capacity;
extern volatile u16 g_bunit_gfx_draw_mode_capacity;

/* Bind both frame buffers' primitive pools, set up their draw/display
 * environments and present twice to establish the active pair.
 *
 * BUNIT twin of world_gfx_init_double_packet_buffers: one pool argument per
 * frame-buffer pointer, each advanced by its capacity for the second buffer. */
void bunit_gfx_init_double_packet_buffers(bunit_gfx_context_t* buffers, u32* otags, POLY_F3* flat_triangles,
    POLY_FT3* textured_triangles, POLY_F4* flat_quads, POLY_FT4* textured_quads, POLY_G3* gouraud_triangles,
    POLY_GT3* gouraud_textured_triangles, POLY_G4* gouraud_quads, POLY_GT4* gouraud_textured_quads, LINE_F2* flat_lines,
    u8 (*flat_lines_3)[0x18], u8 (*flat_lines_4)[0x1c], LINE_G2* gouraud_lines, u8 (*gouraud_lines_3)[0x20],
    u8 (*gouraud_lines_4)[0x28], TILE* tiles, u8 (*tiles_1)[0xc], u8 (*tiles_8)[0xc], u8 (*tiles_16)[0xc],
    SPRT* sprites, u8 (*sprites_8)[0x10], u8 (*sprites_16)[0x10], DR_MOVE* draw_moves, DR_AREA* draw_areas,
    DR_MODE* draw_modes) {
    s32 i;
    bunit_gfx_context_t* buffer;

    g_bunit_gfx_context_base = buffers;
    g_bunit_gfx_context = buffers;
    for (i = 0; i < 2; i++) {
        buffer = &g_bunit_gfx_context_base[i];
        buffer->otag = otags + g_bunit_gfx_otag_count * i;
        buffer->flat_triangles = flat_triangles + g_bunit_gfx_poly_f3_capacity * i;
        buffer->textured_triangles = textured_triangles + g_bunit_gfx_poly_ft3_capacity * i;
        buffer->flat_quads = flat_quads + g_bunit_gfx_poly_f4_capacity * i;
        buffer->textured_quads = textured_quads + g_bunit_gfx_poly_ft4_capacity * i;
        buffer->gouraud_triangles = gouraud_triangles + g_bunit_gfx_poly_g3_capacity * i;
        buffer->gouraud_textured_triangles = gouraud_textured_triangles + g_bunit_gfx_poly_gt3_capacity * i;
        buffer->gouraud_quads = gouraud_quads + g_bunit_gfx_poly_g4_capacity * i;
        buffer->gouraud_textured_quads = gouraud_textured_quads + g_bunit_gfx_poly_gt4_capacity * i;
        buffer->flat_lines = flat_lines + g_bunit_gfx_line_f2_capacity * i;
        buffer->flat_lines_3 = flat_lines_3 + g_bunit_gfx_line_f3_capacity * i;
        buffer->flat_lines_4 = flat_lines_4 + g_bunit_gfx_line_f4_capacity * i;
        buffer->gouraud_lines = gouraud_lines + g_bunit_gfx_line_g2_capacity * i;
        buffer->gouraud_lines_3 = gouraud_lines_3 + g_bunit_gfx_line_g3_capacity * i;
        buffer->gouraud_lines_4 = gouraud_lines_4 + g_bunit_gfx_line_g4_capacity * i;
        buffer->tiles = tiles + g_bunit_gfx_tile_capacity * i;
        buffer->tiles_1 = tiles_1 + g_bunit_gfx_tile_1_capacity * i;
        buffer->tiles_8 = tiles_8 + g_bunit_gfx_tile_8_capacity * i;
        buffer->tiles_16 = tiles_16 + g_bunit_gfx_tile_16_capacity * i;
        buffer->sprites = sprites + g_bunit_gfx_sprite_capacity * i;
        buffer->sprites_8 = sprites_8 + g_bunit_gfx_sprite_8_capacity * i;
        buffer->sprites_16 = sprites_16 + g_bunit_gfx_sprite_16_capacity * i;
        buffer->draw_moves = draw_moves + g_bunit_gfx_draw_move_capacity * i;
        buffer->draw_areas = draw_areas + g_bunit_gfx_draw_area_capacity * i;
        buffer->draw_modes = draw_modes + g_bunit_gfx_draw_mode_capacity * i;
        bunit_gfx_init_packet_pools(buffer);
        SetDefDrawEnv(&g_bunit_gfx_context_base[i].draw_env, 0, i * 0xf0, 0xff, 0xf0);
        SetDefDispEnv(&g_bunit_gfx_context_base[i].disp_env, 0, i == 0 ? 0xf0 : 0, 0xff, 0xf0);
        g_bunit_gfx_context_base[i].draw_env.isbg = 0;
        g_bunit_gfx_context_base[i].draw_env.r0 = 0;
        g_bunit_gfx_context_base[i].draw_env.g0 = 0;
        g_bunit_gfx_context_base[i].draw_env.b0 = 0;
        g_bunit_gfx_context_base[i].draw_env.ofs[0] = -0x80;
    }
    bunit_gfx_submit_frame_and_swap_buffers(0, -1);
    bunit_gfx_submit_frame_and_swap_buffers(0, -1);
}
