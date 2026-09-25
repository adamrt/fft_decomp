#include "fft/event_equip.h"
#include "psx/gpu.h"
#include "psx/types.h"

void equip_gfx_init_primitive_buffers(equip_gfx_context_t* context) {
    s32 i;

    for (i = 0; i < (s32)g_equip_gfx_line_f2_capacity; i++) {
        SetLineF2(&context->lines[i]);
    }
    for (i = 0; i < (s32)g_equip_gfx_poly_f4_capacity; i++) {
        SetPolyF4(&context->poly_f4[i]);
        SetShadeTex(&context->poly_f4[i], 0);
    }
    for (i = 0; i < (s32)g_equip_gfx_poly_ft4_capacity; i++) {
        SetPolyFT4(&context->textured_quads[i]);
        SetShadeTex(&context->textured_quads[i], 0);
    }
    for (i = 0; i < (s32)g_equip_gfx_tile_capacity; i++) {
        SetTile(&context->tiles[i]);
    }
    for (i = 0; i < (s32)g_equip_gfx_draw_move_capacity; i++) {
        SetDrawMove(&context->moves[i]);
    }
}
