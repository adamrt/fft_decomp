#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Takes the next DR_MODE packet from the pool, fills it and links it into
 * ordering-table entry ot_index (libgpu addPrim). */
void world_gfx_add_draw_mode_primitive(s32 dfe, s32 dtd, s32 tpage, RECT* tw, s32 ot_index) {
    DR_MODE* mode = &g_world_gfx_active_packet_buffer->draw_modes[g_world_gfx_draw_mode_count++];

    SetDrawMode(mode, dfe, dtd, tpage, tw);
    setaddr(mode, getaddr(&g_world_gfx_active_packet_buffer->otag[ot_index]));
    setaddr(&g_world_gfx_active_packet_buffer->otag[ot_index], mode);
}
