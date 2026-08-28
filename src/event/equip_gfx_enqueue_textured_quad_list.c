#include "fft/equip.h"
#include "psx/types.h"

typedef struct equip_textured_quad_descriptor_t {
    u8 x;
    u8 y;
    u8 w;
    u8 h;
    u8 u;
    u8 v;
} equip_textured_quad_descriptor_t;

void equip_gfx_enqueue_textured_quad_list(const equip_textured_quad_descriptor_t* quads, const u8* color, s32 semitrans,
    u16 texture_page, u16 clut, s32 otag_index, s32 count) {
    u32 command_mask;
    u32 address_mask;
    u32 otag_offset;

    count--;
    if (count < 0) {
        return;
    }

    otag_offset = otag_index << 2;
    address_mask = 0xffffff;
    command_mask = 0xff000000;
    do {
        POLY_FT4* poly;
        u16 index = g_equip_gfx_poly_ft4_count;

        g_equip_gfx_poly_ft4_count = index + 1;
        poly = &g_equip_gfx_context->textured_quads[index];

        if (color != 0) {
            SetShadeTex(poly, 0);
            poly->r0 = color[0];
            poly->g0 = color[1];
            poly->b0 = color[2];
        } else {
            SetShadeTex(poly, 1);
        }
        SetSemiTrans(poly, semitrans);

        poly->x0 = quads->x + 0x80;
        poly->y0 = quads->y;
        poly->x1 = quads->w + (quads->x + 0x80);
        poly->y1 = quads->y;
        poly->x2 = quads->x + 0x80;
        poly->y2 = quads->y + quads->h;
        poly->x3 = quads->w + (quads->x + 0x80);
        poly->y3 = quads->y + quads->h;

        poly->u0 = quads->u;
        poly->v0 = quads->v;
        poly->u1 = quads->u + quads->w;
        poly->v1 = quads->v;
        poly->u2 = quads->u;
        poly->v2 = quads->v + quads->h;
        poly->u3 = quads->u + quads->w;
        poly->v3 = quads->v + quads->h;
        poly->clut = clut;
        poly->tpage = texture_page;

        {
            equip_gfx_context_t* context = g_equip_gfx_context;
            u32 tag_word = poly->tag;
            u32 otag_word = *(u32*)(otag_offset + (u32)context->otag);

            tag_word &= command_mask;
            otag_word &= address_mask;
            tag_word |= otag_word;
            poly->tag = tag_word;

            {
                u32 old_otag = *(u32*)(otag_offset + (u32)context->otag);
                u32 poly_address = (u32)poly & address_mask;

                old_otag &= command_mask;
                old_otag |= poly_address;
                *(u32*)(otag_offset + (u32)context->otag) = old_otag;
            }
        }

        quads++;
        count--;
    } while (count >= 0);
}
