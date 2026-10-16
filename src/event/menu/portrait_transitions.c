/* Portrait polygon clips and the status banner cross-fade primitives. */

#include "fft/gfx.h"
#include "fft/memory.h"
#include "fft/menu_panel.h"
#include "fft/script.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Narrow a portrait quad by amount pixels from its right edge, moving its texture start to keep
 * the image aligned (along v for a rotated texture). */
static void gfx_clip_portrait_poly_from_right(POLY_FT4* poly, s32 amount) {
    if (poly->u0 != poly->u1) {
        poly->u0 += amount;
        poly->u2 += amount;
    } else {
        poly->v0 += amount;
        poly->v2 += amount;
    }
    poly->x1 -= amount;
    poly->x3 -= amount;
}

/* Move the left edge right and trim the trailing texture coordinates. */
static void gfx_clip_portrait_poly_from_left(POLY_FT4* poly, s32 amount) {
    poly->x0 += amount;
    poly->x2 += amount;
    if (poly->u0 != poly->u1) {
        poly->u1 -= amount;
        poly->u3 -= amount;
    } else {
        poly->v1 -= amount;
        poly->v3 -= amount;
    }
}

/*
 * Build the primitives of the status banner's portrait cross-fade.
 * Each scene binds its own portrait resolver for the supplied identifiers.
 *
 * *transition counts the steps: 0 shows the first portrait, 1 the second,
 * 2-3 capture both CLUTs, 4-9 wipe between the two portraits with two opaque
 * and two semi-transparent quads; from step 2 on it advances by the event
 * speed. `direction` 1 brings the first portrait in from the right as the
 * second leaves to the left; anything else mirrors that. `i` serves the
 * vertex loop, the CLUT loops and the wipe step alike.
 */
void gfx_build_portrait_transition_primitives(const RECT* texture_rect, s32* transition, const s32* first_portrait,
    const s32* second_portrait, palette_t palettes[2], POLY_FT4* poly, s32 direction) {
    RECT second_rect;
    RECT first_rect;
    s32 i;

    mem_copy_bytes_nonempty(&first_rect, texture_rect, sizeof(first_rect));
    mem_copy_bytes_nonempty(&second_rect, texture_rect, sizeof(second_rect));
    second_rect.x += 0x10;

    for (i = 0; i < 4; i++) {
        setXY4(&poly[i], g_menu_portrait_origin.x + 2, g_menu_portrait_origin.y + 2, g_menu_portrait_origin.x + 0x21,
            g_menu_portrait_origin.y + 2, g_menu_portrait_origin.x + 2, g_menu_portrait_origin.y + 0x32,
            g_menu_portrait_origin.x + 0x21, g_menu_portrait_origin.y + 0x32);
    }

    if (*transition == 0) {
        menu_build_portrait_poly_variant(*first_portrait, poly);
        return;
    }
    if (*transition == 1) {
        menu_build_portrait_poly_variant(*second_portrait, poly);
        *transition += 1;
        return;
    }
    if (*transition < 4) {
        menu_build_portrait_poly_variant(*second_portrait, poly);
        gfx_set_clut_rect_from_id(&second_rect, poly->clut);
        poly++;
        menu_build_portrait_poly_variant(*first_portrait, poly);
        gfx_set_clut_rect_from_id(&first_rect, poly->clut);
        StoreImage(&first_rect, (u32*)palettes[0]);
        StoreImage(&second_rect, (u32*)palettes[1]);
        *transition += g_script_event_speed;
    } else if (*transition < 10) {
        if (*transition == 4) {
            for (i = 0; i < 16; i++) {
                palettes[0][i].semi_transparent = 1;
            }
            for (i = 0; i < 16; i++) {
                palettes[1][i].semi_transparent = 1;
            }
            LoadImage(&first_rect, (u32*)palettes[0]);
            LoadImage(&second_rect, (u32*)palettes[1]);
        }

        i = *transition - 4;
        menu_build_portrait_poly_variant(*second_portrait, poly);
        if (direction == 1) {
            gfx_clip_portrait_poly_from_right(poly, g_menu_portrait_transition_offsets[i]);
        } else {
            gfx_clip_portrait_poly_from_left(poly, g_menu_portrait_transition_offsets[i]);
        }
        poly++;

        menu_build_portrait_poly_variant(*first_portrait, poly);
        if (direction == 1) {
            gfx_clip_portrait_poly_from_left(poly, 0x1f - g_menu_portrait_transition_offsets[i]);
        } else {
            gfx_clip_portrait_poly_from_right(poly, 0x1f - g_menu_portrait_transition_offsets[i]);
        }
        poly++;

        menu_build_portrait_poly_variant(*second_portrait, poly);
        poly->tpage = (poly->tpage & 0xff9f) | 0x20;
        poly->clut = GetClut(second_rect.x, second_rect.y);
        SetShadeTex(poly, 0);
        SetSemiTrans(poly, 1);
        if (direction == 1) {
            gfx_clip_portrait_poly_from_right(poly, g_menu_portrait_transition_offsets[i + 8]);
        } else {
            gfx_clip_portrait_poly_from_left(poly, g_menu_portrait_transition_offsets[i + 8]);
        }
        poly++;

        menu_build_portrait_poly_variant(*first_portrait, poly);
        poly->tpage = (poly->tpage & 0xff9f) | 0x20;
        poly->clut = GetClut(first_rect.x, first_rect.y);
        SetShadeTex(poly, 0);
        SetSemiTrans(poly, 1);
        if (direction == 1) {
            gfx_clip_portrait_poly_from_left(poly, 0x1f - g_menu_portrait_transition_offsets[i + 8]);
        } else {
            gfx_clip_portrait_poly_from_right(poly, 0x1f - g_menu_portrait_transition_offsets[i + 8]);
        }
        *transition += g_script_event_speed;
    }
}
