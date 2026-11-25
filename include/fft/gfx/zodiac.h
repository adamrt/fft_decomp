#ifndef ZODIAC_H
#define ZODIAC_H

/* Zodiac signs: their compatibility, the tables that give a birthday its sign, and the scaled zodiac pictures. */

#include "fft/gfx/gfx.h"
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

/* zodiac pictures */
/* Draw state shared by the resident, formation and deployment ZODIAC renderers. */
typedef struct zodiac_draw_context {
    u32* ot;             /* 0x00: ordering table entry the primitives link into */
    s32 brightness;      /* 0x04: low byte is r0/g0/b0 of every sprite; WORLD stores g_menu_screen_brightness_red */
    s32 link_primitives; /* 0x08: nonzero: link each sprite into the OT */
    s16 scale_x;         /* 0x0c: signed scale with 12 fractional bits; ONE = 1.0 */
    s16 scale_y;         /* 0x0e: signed scale with 12 fractional bits; ONE = 1.0 */
    u8 _unused_10[8];
    sprite_draw_offset_t offset; /* 0x18 */
} zodiac_draw_context_t;
typedef char zodiac_draw_context_size_must_be_36[sizeof(zodiac_draw_context_t) == 36 ? 1 : -1];

void gfx_load_zodiac_and_frame_textures(void);

/* The scaled zodiac sprites and their backdrop. */
void zodiac_build_primitives(zodiac_draw_context_t* context, POLY_FT4* poly);
void zodiac_build_resident_primitives(zodiac_draw_context_t* context, POLY_FT4* poly);

/* backdrop */
/* The scrolling zodiac backdrop behind the Formation and save screens. */
void zodiac_backdrop_set_otag(u32* ordering_table);
void zodiac_backdrop_thread(void);

#endif
