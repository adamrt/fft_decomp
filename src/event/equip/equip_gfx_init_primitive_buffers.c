#include "fft/event_equip.h"
#include "fft/gfx.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Initialize the LINE_F2, POLY_F4, POLY_FT4, TILE and DR_MOVE primitive
 * headers of one graphics context, up to each pool's capacity; the quads
 * also get texture shading enabled. */
void equip_gfx_init_primitive_buffers(menu_gfx_context_t* context) {
    s32 i;

    for (i = 0; i < g_equip_gfx_line_f2_capacity; i++) {
        SetLineF2(&context->flat_lines[i]);
    }
    for (i = 0; i < g_equip_gfx_poly_f4_capacity; i++) {
        SetPolyF4(&context->flat_quads[i]);
        SetShadeTex(&context->flat_quads[i], 0);
    }
    for (i = 0; i < g_equip_gfx_poly_ft4_capacity; i++) {
        SetPolyFT4(&context->textured_quads[i]);
        SetShadeTex(&context->textured_quads[i], 0);
    }
    for (i = 0; i < g_equip_gfx_tile_capacity; i++) {
        SetTile(&context->tiles[i]);
    }
    for (i = 0; i < g_equip_gfx_draw_move_capacity; i++) {
        SetDrawMove(&context->draw_moves[i]);
    }
}
