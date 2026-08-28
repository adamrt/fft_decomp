#include "fft/world.h"
#include "psx/gpu.h"

/* Append a two-color line to the menu ordering table. */
void world_menu_add_gradient_line_primitive(s16* endpoints, u8* rgb, s32 semi_trans, s32 ot_index) {
    LINE_G2* line = &g_world_gfx_active_packet_buffer->gradient_lines[g_world_gfx_gradient_line_count++];
    SetSemiTrans(line, semi_trans);
    line->x0 = endpoints[0] + 0x80;
    line->y0 = endpoints[1];
    line->x1 = endpoints[2] + 0x80;
    line->y1 = endpoints[3];
    line->r0 = rgb[0];
    line->g0 = rgb[1];
    line->b0 = rgb[2];
    line->r1 = rgb[3];
    line->g1 = rgb[4];
    line->b1 = rgb[5];
    setaddr(line, getaddr(&g_world_gfx_active_packet_buffer->otag[ot_index]));
    setaddr(&g_world_gfx_active_packet_buffer->otag[ot_index], line);
}
