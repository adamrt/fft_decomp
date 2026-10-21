#ifndef ZODIAC_H
#define ZODIAC_H

/* Zodiac signs: their compatibility, the tables that give a birthday its sign, and the scaled zodiac pictures. */

#include "fft/gfx.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* signs */
/* The compatibility table is indexed by absolute zodiac-ID difference, not
 * by a particular sign. Value 3 is resolved from sex and monster flags. */
typedef enum zodiac_compatibility {
    ZODIAC_COMPATIBILITY_NEUTRAL = 0,
    ZODIAC_COMPATIBILITY_BAD = 1,
    ZODIAC_COMPATIBILITY_GOOD = 2,
    ZODIAC_COMPATIBILITY_GENDER_DEPENDENT = 3,
    ZODIAC_COMPATIBILITY_WORST = 4,
    ZODIAC_COMPATIBILITY_BEST = 5
} zodiac_compatibility_e;

s32 zodiac_sign_calculate(u16 birthday);

/* zodiac pictures */
extern upoint16_t g_zodiac_screen_position;

void zodiac_frame_load(void);

/* The scaled zodiac sprites and their backdrop. */
void zodiac_build_primitives(zodiac_draw_context_t* context, POLY_FT4* poly);
void zodiac_resident_primitives_build(zodiac_draw_context_t* context, POLY_FT4* poly);
void zodiac_build_sprite_polygon(POLY_FT4* poly, point16_t* texture_page, const RECT* position,
    sprite_sheet_frame_t* sprite_rect, point16_t* scale, const sprite_draw_offset_t* offset);

#endif
