#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Initializes every primitive pool of a packet buffer with its libgpu
 * constructor; quads additionally get texture shading enabled. */
void world_gfx_init_packet_pools(world_gfx_packet_buffer_t* buffer) {
    s32 i;

    for (i = 0; i < g_world_gfx_textured_quad_capacity; i++) {
        SetPolyFT4(&buffer->textured_quads[i]);
        SetShadeTex(&buffer->textured_quads[i], 0);
    }
    for (i = 0; i < g_world_gfx_gradient_quad_capacity; i++) {
        SetPolyG4(&buffer->gradient_quads[i]);
        SetShadeTex(&buffer->gradient_quads[i], 0);
    }
    for (i = 0; i < g_world_gfx_textured_gradient_quad_capacity; i++) {
        SetPolyGT4(&buffer->textured_gradient_quads[i]);
        SetShadeTex(&buffer->textured_gradient_quads[i], 0);
    }
    for (i = 0; i < g_world_gfx_tiles_24_capacity; i++) {
        SetLineF2(&buffer->tiles_24[i]);
    }
    for (i = 0; i < g_world_gfx_gradient_line_capacity; i++) {
        SetLineG2(&buffer->gradient_lines[i]);
    }
    for (i = 0; i < g_world_gfx_tile_capacity; i++) {
        SetTile(&buffer->tiles[i]);
    }
    for (i = 0; i < g_world_gfx_draw_move_capacity; i++) {
        SetDrawMove(&buffer->draw_moves[i]);
    }
}
