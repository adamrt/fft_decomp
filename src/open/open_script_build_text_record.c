#include "fft/open.h"
#include "psx/gpu.h"
#include "psx/types.h"

typedef struct open_font_metrics {
    /* 0x00 */ u8 u;
    /* 0x01 */ u8 v;
    /* 0x02 */ u8 flags;
    /* 0x03 */ u8 bearing;
    /* 0x04 */ u8 height;
    /* 0x05 */ u8 width;
    /* 0x06 */ u8 descender;
    /* 0x07 */ u8 advance;
} open_font_metrics_t;

extern open_font_metrics_t* g_open_script_font_metrics;

#define G g_open_script_state.records[index].glyphs[drawn]

/* Lays out one OPEN script text line as textured glyph quads.
 *
 * Claims the first free script record, then walks the text until '@' or 42
 * glyphs. Escape characters followed by one hex digit select the font bank
 * ('$'), CLUT row ('^'), pen adjustments ('<', '>', '+'), the record field at
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
        if (!(g_open_script_state.records[i].flags & 1)) {
            break;
        }
    }
    if (i == 16) {
        return 0;
    }
    index = i;

    g_open_script_state.records[index].flags = 1;
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
            g_open_script_state.dispatch.font_bank = value;
            i--;
        } else if (ch == '^') {
            text++;
            value = open_text_decode_hex_digit(*text++);
            g_open_script_state.dispatch.clut = GetClut(value * 16, 0x1e0) & 0xffff;
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
            value = (ch - 0x20) * 8 + g_open_script_state.dispatch.font_bank * 0x380;
            mt = (open_font_metrics_t*)(value + (s32)font);
            if (mt->flags & 1) {
                text++;
                i--;
                pen += mt->width;
            } else {
                G.x = pen - mt->bearing;
                G.y = -mt->descender;
                width = mt->bearing + mt->width;
                height = mt->height + mt->descender - 1;
                G.width = width;
                G.height = height;
                if (g_open_script_state.dispatch.wide_glyphs == 0) {
                    G.x = G.x * 2;
                    G.width = G.width * 2;
                } else {
                    G.x = G.x + G.x / 2;
                    G.width = G.width + G.width / 2;
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
                front->r0 = 0x80;
                front->g0 = 0x80;
                front->b0 = 0x80;
                back->r0 = 0x80;
                back->g0 = 0x80;
                back->b0 = 0x80;
                mt2 = (open_font_metrics_t*)((u8*)font + value);
                u0 = mt2->u;
                v0 = mt2->v - mt2->height - mt2->descender + 1;
                front->u0 = u0;
                front->v0 = v0;
                front->u1 = u0 + width;
                front->v1 = v0;
                front->u2 = u0;
                front->v2 = v0 + height;
                front->u3 = u0 + width;
                front->v3 = v0 + height;
                back->u0 = u0;
                back->v0 = v0;
                back->u1 = u0 + width;
                back->v1 = v0;
                back->u2 = u0;
                back->v2 = v0 + height;
                back->u3 = u0 + width;
                back->v3 = v0 + height;
                text++;
                slot++;
                pen += mt2->advance;
            }
        }
    }
    g_open_script_state.records[index].glyph_count = drawn;
    return index + 1;
}
