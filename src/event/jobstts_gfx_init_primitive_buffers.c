#include "fft/event_jobstts.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Initialize the POLY_FT4, TILE and DR_MOVE primitive headers of one graphics context. */
void jobstts_gfx_init_primitive_buffers(jobstts_gfx_context_t* context) {
    s32 i;

    for (i = 0; i < g_jobstts_gfx_poly_ft4_capacity; i++) {
        SetPolyFT4(&context->textured_quads[i]);
        SetShadeTex(&context->textured_quads[i], 0);
    }
    for (i = 0; i < g_jobstts_gfx_tile_capacity; i++) {
        SetTile(&context->tiles[i]);
    }
    for (i = 0; i < g_jobstts_gfx_draw_move_capacity; i++) {
        SetDrawMove(&context->draw_moves[i]);
    }
}
