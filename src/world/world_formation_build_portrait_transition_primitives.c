#include "fft/world.h"
#include "psx/gpu.h"

void world_formation_build_portrait_transition_primitives(const RECT* texture_rect, s32* transition,
    const s32* first_portrait, const s32* second_portrait, u8* image, POLY_FT4* poly, s32 direction) {
    RECT second_rect;
    RECT first_rect;
    s32* transition_p = transition;
    const s32* first_portrait_p = first_portrait;
    const s32* second_portrait_p = second_portrait;
    /* Pin required: unpinned, the callee-saved registers of the pointer
     * arguments permute. */
    register u8* image_p __asm__("$20") = image;
    register POLY_FT4* poly_p __asm__("$17") = poly;
    s32 direction_v = direction;
    s32 phase;
    s32 transition_value;
    volatile u8* cursor;

    /* Emits nothing; moves the three pointer arguments into their
     * callee-saved registers at entry, in the target's order and registers. */
    __asm__("" : : "r"(transition_p), "r"(first_portrait_p), "r"(second_portrait_p));
    world_script_copy_bytes(&first_rect, texture_rect, sizeof(first_rect));
    world_script_copy_bytes(&second_rect, texture_rect, sizeof(second_rect));
    second_rect.x += 0x10;

    phase = 0;
    cursor = (u8*)poly_p;
    do {
        *(volatile s16*)(cursor + 8) = g_world_gfx_portrait_origin[0] + 2;
        *(volatile s16*)(cursor + 0xa) = g_world_gfx_portrait_origin[1] + 2;
        *(volatile s16*)(cursor + 0x10) = g_world_gfx_portrait_origin[0] + 0x21;
        *(volatile s16*)(cursor + 0x12) = g_world_gfx_portrait_origin[1] + 2;
        *(volatile s16*)(cursor + 0x18) = g_world_gfx_portrait_origin[0] + 2;
        *(volatile s16*)(cursor + 0x1a) = g_world_gfx_portrait_origin[1] + 0x32;
        *(volatile s16*)(cursor + 0x20) = g_world_gfx_portrait_origin[0] + 0x21;
        phase++;
        *(volatile s16*)(cursor + 0x22) = g_world_gfx_portrait_origin[1] + 0x32;
        cursor += sizeof(POLY_FT4);
    } while (phase < 4);

    if (*transition_p == 0) {
        world_build_portrait_poly_ft4(*first_portrait_p, poly_p);
        return;
    }
    if (*transition_p == 1) {
        world_build_portrait_poly_ft4(*second_portrait_p, poly_p);
        *transition_p += 1;
        return;
    }
    if (*transition_p < 4) {
        world_build_portrait_poly_ft4(*second_portrait_p, poly_p);
        world_gfx_set_clut_rect_from_id(&second_rect, poly_p->clut);
        poly_p++;
        world_build_portrait_poly_ft4(*first_portrait_p, poly_p);
        world_gfx_set_clut_rect_from_id(&first_rect, poly_p->clut);
        StoreImage(&first_rect, (u32*)image_p);
        StoreImage(&second_rect, (u32*)(image_p + 0x20));
    } else if (*transition_p < 10) {
        if (*transition_p == 4) {
            volatile u16* pixel;

            phase = 0;
            pixel = (u16*)image_p;
            do {
                phase++;
                *pixel |= 0x8000;
                pixel++;
            } while (phase < 0x10);
            phase = 0;
            pixel = (u16*)image_p;
            do {
                phase++;
                pixel[16] |= 0x8000;
                pixel++;
            } while (phase < 0x10);
            LoadImage(&first_rect, (u32*)image_p);
            LoadImage(&second_rect, (u32*)(image_p + 0x20));
        }

        transition_value = *transition_p;
        phase = transition_value - 4;
        world_build_portrait_poly_ft4(*second_portrait_p, poly_p);
        if (direction_v == 1)
            world_gfx_clip_sprite_record_left(poly_p, g_world_gfx_portrait_transition_offsets[phase]);
        else
            world_gfx_clip_sprite_record_right(poly_p, g_world_gfx_portrait_transition_offsets[phase]);
        poly_p++;

        world_build_portrait_poly_ft4(*first_portrait_p, poly_p);
        if (direction_v == 1)
            world_gfx_clip_sprite_record_right(poly_p, 0x1f - g_world_gfx_portrait_transition_offsets[phase]);
        else
            world_gfx_clip_sprite_record_left(poly_p, 0x1f - g_world_gfx_portrait_transition_offsets[phase]);
        poly_p++;

        world_build_portrait_poly_ft4(*second_portrait_p, poly_p);
        poly_p->tpage = (poly_p->tpage & 0xff9f) | 0x20;
        poly_p->clut = GetClut(second_rect.x, second_rect.y);
        SetShadeTex(poly_p, 0);
        SetSemiTrans(poly_p, 1);
        if (direction_v == 1)
            world_gfx_clip_sprite_record_left(poly_p, g_world_gfx_portrait_transition_offsets[phase + 8]);
        else
            world_gfx_clip_sprite_record_right(poly_p, g_world_gfx_portrait_transition_offsets[phase + 8]);
        poly_p++;

        world_build_portrait_poly_ft4(*first_portrait_p, poly_p);
        poly_p->tpage = (poly_p->tpage & 0xff9f) | 0x20;
        poly_p->clut = GetClut(first_rect.x, first_rect.y);
        SetShadeTex(poly_p, 0);
        SetSemiTrans(poly_p, 1);
        if (direction_v == 1)
            world_gfx_clip_sprite_record_right(poly_p, 0x1f - g_world_gfx_portrait_transition_offsets[phase + 8]);
        else
            world_gfx_clip_sprite_record_left(poly_p, 0x1f - g_world_gfx_portrait_transition_offsets[phase + 8]);
    } else {
        return;
    }

    *transition_p += g_world_event_speed;
}
