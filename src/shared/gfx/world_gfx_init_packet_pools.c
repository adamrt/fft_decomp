#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Initializes every primitive pool of a packet buffer with its libgpu
 * constructor; quads additionally get texture shading enabled. */
void world_gfx_init_packet_pools(menu_gfx_context_t* buffer) {
    s32 i;

    for (i = 0; i < g_world_gfx_textured_quad_capacity; i++) {
        SetPolyFT4(&buffer->textured_quads[i]);
        SetShadeTex(&buffer->textured_quads[i], 0);
    }
    for (i = 0; i < g_world_gfx_gradient_quad_capacity; i++) {
        SetPolyG4(&buffer->gouraud_quads[i]);
        SetShadeTex(&buffer->gouraud_quads[i], 0);
    }
    for (i = 0; i < g_world_gfx_textured_gradient_quad_capacity; i++) {
        SetPolyGT4(&buffer->gouraud_textured_quads[i]);
        SetShadeTex(&buffer->gouraud_textured_quads[i], 0);
    }
    for (i = 0; i < g_world_gfx_flat_line_capacity; i++) {
        SetLineF2(&buffer->flat_lines[i]);
    }
    for (i = 0; i < g_world_gfx_gradient_line_capacity; i++) {
        SetLineG2(&buffer->gouraud_lines[i]);
    }
    for (i = 0; i < g_gfx_tile_capacity; i++) {
        SetTile(&buffer->tiles[i]);
    }
    for (i = 0; i < g_world_gfx_draw_move_capacity; i++) {
        SetDrawMove(&buffer->draw_moves[i]);
    }
}
