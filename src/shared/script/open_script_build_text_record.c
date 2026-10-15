#include "fft/open.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Lays out one OPEN script text line as textured glyph quads.
 *
 * Claims the first free script record, then walks the text until '@' or 42
 * glyphs. Escape characters followed by one hex digit select the font
 * ('$'), CLUT ('^'), pen adjustments ('<', '>', '+'), the record field at
 * 0x08 ('/') and wide-glyph mode ('*'). Every other character emits a glyph
 * record and a POLY_FT4 in both graphics buffers. Returns the record index
 * plus 1, or 0 when all 16 records are busy.
 *
 * `value` holds both the decoded escape digits and the glyph metrics offset:
 * the extra references give it the target's global-allocation priority
 * (s3, ahead of the loop counter). The second metrics pointer is a separate
 * variable so the first stays in a0.
 */
s32 open_script_build_text_record(u8* text) {
    s32 index;
    s32 i;
    s32 drawn;
    s32 pen;
    s32 width;
    s32 height;
    open_font_metrics_t* font;
    open_font_metrics_t* mt;
    open_font_metrics_t* mt2;
    s32 value;
    s32 slot;
    POLY_FT4* back;
    POLY_FT4* front;
    s32 u0;
    s32 v0;
    u8 ch;
    u16 tpage;

    font = g_open_script_font_metrics;
    for (i = 0; i < 16; i++) {
        if (!(g_open_script_state.records[i].flags & OPEN_SCRIPT_RECORD_ACTIVE)) {
            break;
        }
    }
    if (i == 16) {
        return 0;
    }
    index = i;

    g_open_script_state.records[index].flags = OPEN_SCRIPT_RECORD_ACTIVE;
    pen = 0;
    slot = index * 42;
    drawn = 0;
    for (i = 0; i < 42; i++) {
        ch = *text;
        if (ch == '@') {
            break;
        }
        if (ch == '$') {
            text++;
            value = open_text_decode_hex_digit(*text++);
            g_open_script_state.dispatch.font_index = value;
            i--;
        } else if (ch == '^') {
            text++;
            value = open_text_decode_hex_digit(*text++);
            g_open_script_state.dispatch.clut = GetClut(value * 16, 0x1e0);
            i--;
        } else if (ch == '<') {
            text++;
            value = open_text_decode_hex_digit(*text++);
            pen -= value;
            i--;
        } else if (ch == '>') {
            text++;
            value = open_text_decode_hex_digit(*text++);
            pen += value;
            i--;
        } else if (ch == '/') {
            text++;
            value = open_text_decode_hex_digit(*text++);
            g_open_script_state.dispatch.skew = value;
            i--;
        } else if (ch == '*') {
            text++;
            value = open_text_decode_hex_digit(*text++);
            g_open_script_state.dispatch.wide_glyphs = value;
            i--;
        } else if (ch == '+') {
            text++;
            pen = open_text_decode_hex_digit(*text++) * 20;
            i--;
        } else {
            value = (ch - 0x20) * 8 + g_open_script_state.dispatch.font_index * 0x380;
            /* Typed pointer addition reverses this retail addu's operands. */
            mt = (open_font_metrics_t*)(value + (s32)font);
            if (mt->flags & 1) {
                text++;
                i--;
                pen += mt->width;
            } else {
                g_open_script_state.records[index].glyphs[drawn].x = pen - mt->bearing;
                g_open_script_state.records[index].glyphs[drawn].y = -mt->descender;
                width = mt->bearing + mt->width;
                height = mt->height + mt->descender - 1;
                g_open_script_state.records[index].glyphs[drawn].w = width;
                g_open_script_state.records[index].glyphs[drawn].h = height;
                if (g_open_script_state.dispatch.wide_glyphs == 0) {
                    g_open_script_state.records[index].glyphs[drawn].x
                        = g_open_script_state.records[index].glyphs[drawn].x * 2;
                    g_open_script_state.records[index].glyphs[drawn].w
                        = g_open_script_state.records[index].glyphs[drawn].w * 2;
                } else {
                    g_open_script_state.records[index].glyphs[drawn].x
                        = g_open_script_state.records[index].glyphs[drawn].x
                        + g_open_script_state.records[index].glyphs[drawn].x / 2;
                    g_open_script_state.records[index].glyphs[drawn].w
                        = g_open_script_state.records[index].glyphs[drawn].w
                        + g_open_script_state.records[index].glyphs[drawn].w / 2;
                }
                front = &g_open_script_glyph_primitives[0][slot];
                g_open_script_state.records[index].skew = g_open_script_state.dispatch.skew;
                SetPolyFT4(front);
                back = &g_open_script_glyph_primitives[1][slot];
                SetPolyFT4(back);
                SetSemiTrans(front, 1);
                SetSemiTrans(back, 1);
                tpage = g_open_script_state.dispatch.tpage;
                front[16 * 42].tpage = tpage;
                g_open_script_glyph_primitives[0][slot].tpage = tpage;
                drawn++;
                tpage = g_open_script_state.dispatch.clut;
                front[16 * 42].clut = tpage;
                g_open_script_glyph_primitives[0][slot].clut = tpage;
                setRGB0(front, 0x80, 0x80, 0x80);
                setRGB0(back, 0x80, 0x80, 0x80);
                mt2 = font + (value >> 3);
                u0 = mt2->u;
                v0 = mt2->v - mt2->height - mt2->descender + 1;
                setUV4(front, u0, v0, u0 + width, v0, u0, v0 + height, u0 + width, v0 + height);
                setUV4(back, u0, v0, u0 + width, v0, u0, v0 + height, u0 + width, v0 + height);
                text++;
                slot++;
                pen += mt2->advance;
            }
        }
    }
    g_open_script_state.records[index].glyph_count = drawn;
    return index + 1;
}
