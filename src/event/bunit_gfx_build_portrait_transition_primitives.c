#include "fft/event_bunit.h"

void bunit_gfx_build_portrait_transition_primitives(const RECT* texture_rect, s32* transition,
    const s32* first_portrait, const s32* second_portrait, u8* image, POLY_FT4* poly, s32 direction) {
    RECT second_rect;
    RECT first_rect;
    POLY_FT4* quad;
    u16* pixel;
    s32 i;

    battle_copy_bytes(&first_rect, texture_rect, sizeof(first_rect));
    battle_copy_bytes(&second_rect, texture_rect, sizeof(second_rect));
    second_rect.x += 0x10;

    for (i = 0; i < 4; i++) {
        poly[i].x0 = g_bunit_gfx_portrait_origin.coordinates[0] + 2;
        poly[i].y0 = g_bunit_gfx_portrait_origin.coordinates[1] + 2;
        poly[i].x1 = g_bunit_gfx_portrait_origin.coordinates[0] + 0x21;
        poly[i].y1 = g_bunit_gfx_portrait_origin.coordinates[1] + 2;
        poly[i].x2 = g_bunit_gfx_portrait_origin.coordinates[0] + 2;
        poly[i].y2 = g_bunit_gfx_portrait_origin.coordinates[1] + 0x32;
        poly[i].x3 = g_bunit_gfx_portrait_origin.coordinates[0] + 0x21;
        poly[i].y3 = g_bunit_gfx_portrait_origin.coordinates[1] + 0x32;
    }

    if (*transition == 0) {
        bunit_gfx_build_portrait_poly_ft4(*first_portrait, poly);
        return;
    }
    if (*transition == 1) {
        bunit_gfx_build_portrait_poly_ft4(*second_portrait, poly);
        *transition += 1;
        return;
    }
    if (*transition < 4) {
        bunit_gfx_build_portrait_poly_ft4(*second_portrait, poly);
        bunit_gfx_set_clut_rect_from_id(&second_rect, poly->clut);
        poly++;
        bunit_gfx_build_portrait_poly_ft4(*first_portrait, poly);
        bunit_gfx_set_clut_rect_from_id(&first_rect, poly->clut);
        StoreImage(&first_rect, (u32*)image);
        StoreImage(&second_rect, (u32*)(image + 0x20));
        *transition += g_battle_event_speed;
    } else if (*transition < 10) {
        if (*transition == 4) {
            for (i = 0; i < 0x10; i++) {
                ((u16*)image)[i] |= 0x8000;
            }
            for (i = 0; i < 0x10; i++) {
                ((u16*)image)[i + 16] |= 0x8000;
            }
            LoadImage(&first_rect, (u32*)image);
            LoadImage(&second_rect, (u32*)(image + 0x20));
        }

        i = *transition - 4;
        bunit_gfx_build_portrait_poly_ft4(*second_portrait, poly);
        if (direction == 1) {
            bunit_gfx_clip_portrait_poly_from_right(poly, g_bunit_gfx_portrait_transition_offsets[i]);
        } else {
            bunit_gfx_clip_portrait_poly_from_left(poly, g_bunit_gfx_portrait_transition_offsets[i]);
        }
        poly++;

        bunit_gfx_build_portrait_poly_ft4(*first_portrait, poly);
        if (direction == 1) {
            bunit_gfx_clip_portrait_poly_from_left(poly, 0x1f - g_bunit_gfx_portrait_transition_offsets[i]);
        } else {
            bunit_gfx_clip_portrait_poly_from_right(poly, 0x1f - g_bunit_gfx_portrait_transition_offsets[i]);
        }
        poly++;

        bunit_gfx_build_portrait_poly_ft4(*second_portrait, poly);
        poly->tpage = (poly->tpage & 0xff9f) | 0x20;
        poly->clut = GetClut(second_rect.x, second_rect.y);
        SetShadeTex(poly, 0);
        SetSemiTrans(poly, 1);
        if (direction == 1) {
            bunit_gfx_clip_portrait_poly_from_right(poly, g_bunit_gfx_portrait_transition_offsets[i + 8]);
        } else {
            bunit_gfx_clip_portrait_poly_from_left(poly, g_bunit_gfx_portrait_transition_offsets[i + 8]);
        }
        poly++;

        bunit_gfx_build_portrait_poly_ft4(*first_portrait, poly);
        poly->tpage = (poly->tpage & 0xff9f) | 0x20;
        poly->clut = GetClut(first_rect.x, first_rect.y);
        SetShadeTex(poly, 0);
        SetSemiTrans(poly, 1);
        if (direction == 1) {
            bunit_gfx_clip_portrait_poly_from_left(poly, 0x1f - g_bunit_gfx_portrait_transition_offsets[i + 8]);
        } else {
            bunit_gfx_clip_portrait_poly_from_right(poly, 0x1f - g_bunit_gfx_portrait_transition_offsets[i + 8]);
        }
        *transition += g_battle_event_speed;
    }
}
