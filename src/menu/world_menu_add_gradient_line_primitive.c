#include "fft/world.h"
#include "psx/libgpu.h"

/* Append a two-color line to the menu ordering table. */
void world_menu_add_gradient_line_primitive(point16_t* endpoints, rgb24_t* colors, s32 semi_trans, s32 ot_index) {
    LINE_G2* line = &g_menu_gfx_context->gouraud_lines[g_world_gfx_gradient_line_count++];
    SetSemiTrans(line, semi_trans);
    line->x0 = endpoints[0].x + 0x80;
    line->y0 = endpoints[0].y;
    line->x1 = endpoints[1].x + 0x80;
    line->y1 = endpoints[1].y;
    setRGB0(line, colors[0].r, colors[0].g, colors[0].b);
    setRGB1(line, colors[1].r, colors[1].g, colors[1].b);
    addPrim(&g_menu_gfx_context->otag[ot_index], line);
}
