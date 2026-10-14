#include "fft/open.h"
#include "psx/libgpu.h"

/* Queue the ending script's primitives for one frame while the script runs: the
 * OPNTEX picture quad (unless its fade intensity is 0), the two edge bands and
 * one POLY_FT4 per glyph of every active text record.
 *
 * Glyph positions are relative to the center of the 512x240 screen, and skew
 * shifts the top edge of each glyph. The AddPrim calls pass the contents of
 * otag[15] and otag[8], not their addresses: after ClearOTagR those are the
 * addresses of entries 14 and 7, which is where the primitives are linked. */
void open_script_draw_text_records(u32* otag, s32 graphics_buffer_index) {
    s32 i;
    s32 j;
    s32 prim_index;
    s32 x;
    s32 y;

    if (g_open_script_state.dispatch.flags & OPEN_SCRIPT_DISPATCH_RUNNING) {
        if (g_open_script_state.opntex.fade_intensity != 0) {
            AddPrim((void*)otag[15], &g_open_gfx_fade_primitives[graphics_buffer_index]);
        }
        AddPrim((void*)otag[8], &g_open_gfx_credit_band_primitives[0]);
        AddPrim((void*)otag[8], &g_open_gfx_credit_band_primitives[1]);
        for (i = 0; i < 16; i++) {
            if (g_open_script_state.records[i].flags & OPEN_SCRIPT_RECORD_ACTIVE) {
                prim_index = i * 42;
                for (j = 0; j < g_open_script_state.records[i].glyph_count; j++, prim_index++) {
                    x = g_open_script_state.records[i].x + g_open_script_state.records[i].glyphs[j].x - 256;
                    y = g_open_script_state.records[i].y + g_open_script_state.records[i].glyphs[j].y - 120;
                    g_open_script_glyph_primitives[graphics_buffer_index][prim_index].x0
                        = x + g_open_script_state.records[i].skew;
                    g_open_script_glyph_primitives[graphics_buffer_index][prim_index].y0 = y;
                    g_open_script_glyph_primitives[graphics_buffer_index][prim_index].x1
                        = g_open_script_state.records[i].glyphs[j].w + x + g_open_script_state.records[i].skew;
                    g_open_script_glyph_primitives[graphics_buffer_index][prim_index].y1 = y;
                    g_open_script_glyph_primitives[graphics_buffer_index][prim_index].x2 = x;
                    g_open_script_glyph_primitives[graphics_buffer_index][prim_index].y2
                        = g_open_script_state.records[i].glyphs[j].h + y;
                    g_open_script_glyph_primitives[graphics_buffer_index][prim_index].x3
                        = g_open_script_state.records[i].glyphs[j].w + x;
                    g_open_script_glyph_primitives[graphics_buffer_index][prim_index].y3
                        = g_open_script_state.records[i].glyphs[j].h + y;
                    AddPrim((void*)otag[8], &g_open_script_glyph_primitives[graphics_buffer_index][prim_index]);
                }
            }
        }
    }
}
