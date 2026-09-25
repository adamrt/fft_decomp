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

/* Initializes every primitive pool of one frame buffer with its libgpu
 * constructor; polygons additionally get texture shading enabled.
 *
 * BUNIT twin of world_gfx_init_packet_pools. */
void bunit_gfx_init_packet_pools(bunit_gfx_context_t* buffer) {
    s32 i;

    for (i = 0; i < g_bunit_gfx_poly_f3_capacity; i++) {
        SetPolyF3(&buffer->flat_triangles[i]);
        SetShadeTex(&buffer->flat_triangles[i], 0);
    }
    for (i = 0; i < g_bunit_gfx_poly_ft3_capacity; i++) {
        SetPolyFT3(&buffer->textured_triangles[i]);
        SetShadeTex(&buffer->textured_triangles[i], 0);
    }
    for (i = 0; i < g_bunit_gfx_poly_f4_capacity; i++) {
        SetPolyF4(&buffer->flat_quads[i]);
        SetShadeTex(&buffer->flat_quads[i], 0);
    }
    for (i = 0; i < g_bunit_gfx_poly_ft4_capacity; i++) {
        SetPolyFT4(&buffer->textured_quads[i]);
        SetShadeTex(&buffer->textured_quads[i], 0);
    }
    for (i = 0; i < g_bunit_gfx_poly_g3_capacity; i++) {
        SetPolyG3(&buffer->gouraud_triangles[i]);
        SetShadeTex(&buffer->gouraud_triangles[i], 0);
    }
    for (i = 0; i < g_bunit_gfx_poly_gt3_capacity; i++) {
        SetPolyGT3(&buffer->gouraud_textured_triangles[i]);
        SetShadeTex(&buffer->gouraud_textured_triangles[i], 0);
    }
    for (i = 0; i < g_bunit_gfx_poly_g4_capacity; i++) {
        SetPolyG4(&buffer->gouraud_quads[i]);
        SetShadeTex(&buffer->gouraud_quads[i], 0);
    }
    for (i = 0; i < g_bunit_gfx_poly_gt4_capacity; i++) {
        SetPolyGT4(&buffer->gouraud_textured_quads[i]);
        SetShadeTex(&buffer->gouraud_textured_quads[i], 0);
    }
    for (i = 0; i < g_bunit_gfx_line_f2_capacity; i++) {
        SetLineF2(&buffer->flat_lines[i]);
    }
    for (i = 0; i < g_bunit_gfx_line_f3_capacity; i++) {
        SetLineF3(&buffer->flat_lines_3[i]);
    }
    for (i = 0; i < g_bunit_gfx_line_f4_capacity; i++) {
        SetLineF4(&buffer->flat_lines_4[i]);
    }
    for (i = 0; i < g_bunit_gfx_line_g2_capacity; i++) {
        SetLineG2(&buffer->gouraud_lines[i]);
    }
    for (i = 0; i < g_bunit_gfx_line_g3_capacity; i++) {
        SetLineG3(&buffer->gouraud_lines_3[i]);
    }
    for (i = 0; i < g_bunit_gfx_line_g4_capacity; i++) {
        SetLineG4(&buffer->gouraud_lines_4[i]);
    }
    for (i = 0; i < g_bunit_gfx_tile_capacity; i++) {
        SetTile(&buffer->tiles[i]);
    }
    for (i = 0; i < g_bunit_gfx_tile_1_capacity; i++) {
        SetTile1(&buffer->tiles_1[i]);
    }
    for (i = 0; i < g_bunit_gfx_tile_8_capacity; i++) {
        SetTile8(&buffer->tiles_8[i]);
    }
    for (i = 0; i < g_bunit_gfx_tile_16_capacity; i++) {
        SetTile16(&buffer->tiles_16[i]);
    }
    for (i = 0; i < g_bunit_gfx_sprite_capacity; i++) {
        SetSprt(&buffer->sprites[i]);
    }
    for (i = 0; i < g_bunit_gfx_sprite_8_capacity; i++) {
        SetSprt8(&buffer->sprites_8[i]);
    }
    for (i = 0; i < g_bunit_gfx_sprite_16_capacity; i++) {
        SetSprt16(&buffer->sprites_16[i]);
    }
    for (i = 0; i < g_bunit_gfx_draw_move_capacity; i++) {
        SetDrawMove(&buffer->draw_moves[i]);
    }
}
