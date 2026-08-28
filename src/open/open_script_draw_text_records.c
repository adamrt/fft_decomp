#include "fft/open.h"
#include "psx/gpu.h"

#define P g_open_script_glyph_primitives[graphics_buffer_index][prim_index]

void open_script_draw_text_records(u32* otag, s32 graphics_buffer_index) {
    s32 i;
    s32 j;
    s32 prim_index;
    s32 x;
    s32 y;
    POLY_FT4* prim;

    if (g_open_script_state.dispatch.flags & 1) {
        if (g_open_script_state.opntex.fade_intensity != 0) {
            AddPrim((void*)otag[15], &g_open_gfx_fade_primitives[graphics_buffer_index]);
        }
        AddPrim((void*)otag[8], &g_open_gfx_credit_band_primitives[0]);
        AddPrim((void*)otag[8], &g_open_gfx_credit_band_primitives[1]);
        for (i = 0; i < 16; i++) {
            if (g_open_script_state.records[i].flags & 1) {
                prim_index = i * 42;
                for (j = 0; j < g_open_script_state.records[i].glyph_count; j++, prim_index++) {
                    x = g_open_script_state.records[i].x + g_open_script_state.records[i].glyphs[j].x - 256;
                    y = g_open_script_state.records[i].y + g_open_script_state.records[i].glyphs[j].y - 120;
                    P.x0 = x + g_open_script_state.records[i].skew;
                    P.y0 = y;
                    P.x1 = g_open_script_state.records[i].glyphs[j].width + x + g_open_script_state.records[i].skew;
                    P.y1 = y;
                    P.x2 = x;
                    P.y2 = g_open_script_state.records[i].glyphs[j].height + y;
                    P.x3 = g_open_script_state.records[i].glyphs[j].width + x;
                    P.y3 = g_open_script_state.records[i].glyphs[j].height + y;
                    AddPrim((void*)otag[8], &P);
                }
            }
        }
    }
}
