#include "fft/open.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Draw the text lines of the current new-game prologue entry at the given
 * brightness.
 *
 * Each step of the entry is one line of text: line_texture_rows holds its 16-pixel row
 * in the texture page and line_widths its width in pixels. The lines are
 * centerd, 20 pixels apart, as a block around the middle of the screen. A
 * finished line is drawn at full width and the line in progress up to timer
 * pixels; the pass with i == step adds that line's leading edge, at most 16
 * pixels whose right-hand corners are black, so the text fades in as it is
 * revealed.
 *
 * Every piece is two POLY_GT4s: the text, added to the picture at intensity in
 * ordering-table entry ot_layer, and a copy one pixel down and right, subtracted at
 * half intensity in entry ot_layer + 1.
 *
 * The coordinates are written in setXY4 order (x0, y0, x1, y1, ...) per
 * primitive, and width + 1 goes through its own variable (the target adds left
 * afterwards). */
void open_gfx_draw_transition_line_strips(s32 intensity) {
    POLY_GT4* prims;
    s32 i;
    s32 prim_index;
    s32 width;
    s32 width_plus_one;
    s32 left;
    s32 top_y;
    s32 u;
    s32 v;
    u16 clut;

    for (i = 0; i < g_open_gfx_transition.step + 1; i++) {
        prim_index = i * 2;
        prims = g_open_gfx_transition_line_primitives[g_active_graphics_buffer_index];
        SetPolyGT4(&prims[prim_index]);
        SetPolyGT4(&prims[prim_index + 1]);

        if (i == g_open_gfx_transition.step) {
            /* Leading edge of the line in progress (line i - 1): black on its right. */
            if (g_open_gfx_transition.line_widths[i - 1] < g_open_gfx_transition.timer) {
                continue;
            }
            width = g_open_gfx_transition.line_widths[i - 1] - g_open_gfx_transition.timer;
            if (g_open_gfx_transition.timer < 0) {
                width = g_open_gfx_transition.timer + 0x10;
            } else if (width >= 0x11) {
                width = 0x10;
            }
            u = g_open_gfx_transition.timer;
            if (u < 0) {
                u = 0;
            }
            v = g_open_gfx_transition.line_texture_rows[i - 1] * 0x10;
            setRGB1(&prims[prim_index], 0, 0, 0);
            setRGB1(&prims[prim_index + 1], 0, 0, 0);
            setRGB3(&prims[prim_index], 0, 0, 0);
            setRGB3(&prims[prim_index + 1], 0, 0, 0);
            left = u - g_open_gfx_transition.line_widths[i - 1] / 2;
            top_y = (i - 1) * 0x14 - g_open_gfx_transition.step_count * 10;
        } else {
            /* Line i: the revealed part of the line in progress, or a whole finished line. */
            if (i == g_open_gfx_transition.step - 1) {
                width = g_open_gfx_transition.timer;
            } else {
                width = g_open_gfx_transition.line_widths[i];
            }
            u = 0;
            if (width <= 0) {
                continue;
            }
            v = g_open_gfx_transition.line_texture_rows[i] * 0x10;
            setRGB1(&prims[prim_index], intensity, intensity, intensity);
            setRGB1(&prims[prim_index + 1], intensity / 2, intensity / 2, intensity / 2);
            setRGB3(&prims[prim_index], intensity, intensity, intensity);
            setRGB3(&prims[prim_index + 1], intensity / 2, intensity / 2, intensity / 2);
            left = -(g_open_gfx_transition.line_widths[i] / 2);
            top_y = i * 0x14 - g_open_gfx_transition.step_count * 10;
        }

        setRGB0(&prims[prim_index], intensity, intensity, intensity);
        setRGB0(&prims[prim_index + 1], intensity / 2, intensity / 2, intensity / 2);
        setRGB2(&prims[prim_index], intensity, intensity, intensity);
        setRGB2(&prims[prim_index + 1], intensity / 2, intensity / 2, intensity / 2);

        clut = GetClut(0, 0x1e3);
        prims[prim_index + 1].clut = clut;
        prims[prim_index].clut = clut;
        prims[prim_index].tpage = GetTPage(0, 1, g_open_gfx_transition.tpage_x, g_open_gfx_transition.tpage_y);
        prims[prim_index + 1].tpage = GetTPage(0, 2, g_open_gfx_transition.tpage_x, g_open_gfx_transition.tpage_y);

        setXY4(&prims[prim_index], left, top_y, left + width, top_y, left, top_y + 0x10, left + width, top_y + 0x10);
        width_plus_one = width + 1;
        setXY4(&prims[prim_index + 1], left + 1, top_y + 1, left + width_plus_one, top_y + 1, left + 1, top_y + 0x11,
            left + width_plus_one, top_y + 0x11);
        setUV4(&prims[prim_index], u, v, u + width, v, u, v + 0x10, u + width, v + 0x10);
        setUV4(&prims[prim_index + 1], u, v, u + width, v, u, v + 0x10, u + width, v + 0x10);

        SetSemiTrans(&prims[prim_index], 1);
        SetSemiTrans(&prims[prim_index + 1], 1);
        AddPrim(&g_open_gfx_otags[g_active_graphics_buffer_index][g_open_gfx_transition.ot_layer], &prims[prim_index]);
        AddPrim(&g_open_gfx_otags[g_active_graphics_buffer_index][g_open_gfx_transition.ot_layer + 1],
            &prims[prim_index + 1]);
    }
}
