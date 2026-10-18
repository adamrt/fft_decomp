#ifndef ZODIAC_H
#define ZODIAC_H

/* Zodiac signs: their compatibility, the tables that give a birthday its sign, and the scaled zodiac pictures. */

#include "fft/calendar.h"
#include "fft/gfx.h"
#include "fft/unit.h"
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

/* One inclusive date limit and its zodiac ID. The 13-entry WORLD table has
 * Capricorn at both calendar ends and does not contain Serpentarius. */
typedef struct zodiac_date_limit {
    calendar_date_t date;
    u8 zodiac;
} zodiac_date_limit_t;
typedef char zodiac_date_limit_size_must_be_3[(sizeof(zodiac_date_limit_t) == 3) ? 1 : -1];

enum { ZODIAC_DATE_LIMIT_COUNT = 13 };

s32 zodiac_calculate_sign(u16 birthday);

/* zodiac pictures */
extern upoint16_t g_zodiac_screen_position;

void zodiac_load_frame(void);

/* The scaled zodiac sprites and their backdrop. */
void zodiac_build_primitives(zodiac_draw_context_t* context, POLY_FT4* poly);
void zodiac_call_build_primitives(zodiac_draw_context_t* context, POLY_FT4* poly);
void zodiac_build_sprite_polygon(POLY_FT4* poly, point16_t* texture_page, const RECT* position,
    sprite_sheet_frame_t* sprite_rect, point16_t* scale, const sprite_draw_offset_t* offset);

#endif
