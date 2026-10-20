/* The world-map HUD draws funds, time, the calendar date and the current location label. */

#include "fft/calendar.h"
#include "fft/location.h"
#include "fft/script_variable.h"
#include "fft/system.h"
#include "fft/window.h"
#include "fft/world_map.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/libgs.h"
#include "psx/libgte.h"
#include "psx/types.h"

static void funds_draw(GsOT* ot, GsSPRITE* sprite, GsLINE* line);
static void digits_draw(s32 value, s32 digits, GsSPRITE* sprite, GsOT* ot);
static void filled_digits_draw(s32 value, s32 digits, GsSPRITE* sprite, GsOT* ot, s32 zero_pad);

/* Draws the HUD numeric readout at the context-value display position, laid
 * out by g_hud_value_mode: FUNDS is the labeled,
 * underlined war funds, PLAY_TIME hours, minutes and seconds with separators,
 * PLAIN and LABELED an eight-digit value (LABELED under a small label sprite)
 * and COST_AND_FUNDS a labeled five-digit secondary value plus the FUNDS
 * readout.
 *
 * LABELED falls through into PLAIN, so the case bodies are ordered FUNDS,
 * PLAY_TIME, LABELED, PLAIN, COST_AND_FUNDS to follow the target's jump
 * table. The color triple stores into
 * `separator` precede those into `sprite`. The position read back at the end
 * of case 4 is a plain 32-bit load narrowed by the store, where the chained
 * assignment before the switch reads only the low 16 bits. */
void hud_value_draw(GsOT* ot) {
    GsSPRITE sprite;
    GsSPRITE separator;
    GsLINE line;
    CVECTOR color;

    if (g_system_flags & SYSTEM_FLAG_HIDE_CONTEXT_VALUE) {
        return;
    }

    separator.attribute = sprite.attribute = 0;
    separator.tpage = sprite.tpage = GetTPage(0, 0, 0x3C0, 0x100);
    separator.cy = sprite.cy = 0x1E1;
    world_map_fade_tint_color(&g_hud_color, &color);
    separator.r = color.r;
    separator.g = color.g;
    separator.b = color.b;
    sprite.r = color.r;
    sprite.g = color.g;
    sprite.b = color.b;
    separator.x = sprite.x = g_hud_value_position.x;
    separator.y = sprite.y = g_hud_value_position.y;

    switch (g_hud_value_mode) {
    case HUD_VALUE_FUNDS:
        funds_draw(ot, &sprite, &line);
        break;
    case HUD_VALUE_PLAY_TIME:
        sprite.u = 0x60;
        sprite.v = 0x10;
        sprite.w = 0x14;
        sprite.h = 8;
        sprite.cx = 0x20;
        GsSortFastSprite(&sprite, ot, g_hud_ot_priority);

        separator.u = 0x28;
        separator.v = 0;
        separator.w = 6;
        separator.h = 8;
        separator.cx = 0x20;
        sprite.y += 8;
        separator.y = sprite.y + 4;
        sprite.x -= 8;
        digits_draw(g_hud_value, 4, &sprite, ot);

        separator.x = sprite.x;
        GsSortFastSprite(&separator, ot, g_hud_ot_priority);
        sprite.x += 6;
        filled_digits_draw(g_hud_value_secondary, 2, &sprite, ot, 1);

        separator.x = sprite.x;
        GsSortFastSprite(&separator, ot, g_hud_ot_priority);
        sprite.x += 6;
        filled_digits_draw(g_hud_value_tertiary, 2, &sprite, ot, 1);
        break;
    case HUD_VALUE_LABELED:
        sprite.u = 0xA8;
        sprite.v = 0x20;
        sprite.w = 0xD;
        sprite.h = 9;
        sprite.cx = 0x20;
        GsSortFastSprite(&sprite, ot, g_hud_ot_priority);
        /* fallthrough */
    case HUD_VALUE_PLAIN:
        sprite.x -= 0xC;
        sprite.y += 8;
        digits_draw(g_hud_value, 8, &sprite, ot);
        break;
    case HUD_VALUE_COST_AND_FUNDS:
        sprite.u = 0x60;
        sprite.w = 0x18;
        sprite.h = 8;
        sprite.v = 0;
        sprite.cx = 0x20;
        sprite.x -= 0x40;
        GsSortFastSprite(&sprite, ot, g_hud_ot_priority);

        sprite.x += 4;
        sprite.y += 8;
        filled_digits_draw(g_hud_value_secondary, 5, &sprite, ot, 0);

        sprite.x = g_hud_value_position.x;
        sprite.y = g_hud_value_position.y;
        funds_draw(ot, &sprite, &line);
        break;
    }
}

/* Draws the FUNDS readout: the 41x8 label sprite at texel (0x9c, 0x78) with a
 * short black line under its right end, then g_hud_value
 * in eight columns offset by (4, 8), through
 * filled_digits_draw. */
static void funds_draw(GsOT* ot, GsSPRITE* sprite, GsLINE* line) {
    /* u16: the target reads GsSPRITE's short x/y with lhu. */
    u16 x;
    u16 y;

    sprite->u = 0x9C;
    sprite->v = 0x78;
    sprite->w = 0x29;
    sprite->h = 8;
    sprite->cx = 0x20;
    GsSortFastSprite(sprite, ot, g_hud_ot_priority);

    line->attribute = 0;
    line->r = 0;
    line->g = 0;
    line->b = 0;
    x = sprite->x;
    line->x0 = x + 0x24;
    line->x1 = x + 0x28;
    y = sprite->y + 8;
    line->y1 = y;
    line->y0 = y;
    GsSortLine(line, ot, g_hud_ot_priority);

    sprite->x += 4;
    sprite->y += 8;
    filled_digits_draw(g_hud_value, 8, sprite, ot, 0);
}

/* Draws value right-aligned as digits glyphs, one 8x12 or 8x15 cell per
 * digit, stepping the sprite eight pixels right per column. Leading zeros are
 * skipped: a column only draws once the running divisor has dropped to or
 * below value, and the last column always draws. Digit d's glyph is at
 * u 0xa8 + 8 * d; 0 to 5 are 12 pixels tall and 6 to 9 are 15, each placed so
 * the glyphs share a bottom edge.
 *
 * The target wraps `value / divisor` in ASPSX's zero and INT_MIN traps
 * (divcheck expansion). Splitting the division and the remainder into two
 * statements gives the target's order; `s32 saved_y` (not s16) makes the
 * saved y load `lh`. */
static void digits_draw(s32 value, s32 digits, GsSPRITE* sprite, GsOT* ot) {
    s32 divisor = 1;
    s32 i;
    s32 digit;
    s32 saved_y;

    sprite->w = 8;
    sprite->v = 0x31;
    saved_y = sprite->y;
    sprite->cx = 0x70;

    for (i = 0; i < digits - 1; i++) {
        divisor = divisor * 10;
    }

    for (i = 0; i < digits;) {
        if (value >= divisor || i == digits - 1) {
            digit = value / divisor;
            digit = digit % 10;
            sprite->u = digit * 8 - 0x58;
            if (digit < 6) {
                sprite->v = 0x34;
                sprite->h = 0xc;
                sprite->y = sprite->y + 4;
            } else {
                sprite->v = 0x31;
                sprite->h = 0xf;
                sprite->y = sprite->y + 1;
            }
            GsSortFastSprite(sprite, ot, g_hud_ot_priority);
            sprite->y = saved_y;
        }
        sprite->x = sprite->x + 8;
        divisor = divisor / 10;
        i++;
    }
}

/* Draws value right-aligned in `digits` columns like
 * digits_draw, but also draws the columns ahead of the
 * first significant digit: a zero glyph when `zero_pad` is set, otherwise the
 * 8x8 filler glyph at (0xd0, 0x28). */
static void filled_digits_draw(s32 value, s32 digits, GsSPRITE* sprite, GsOT* ot, s32 zero_pad) {
    s32 divisor = 1;
    s32 y;
    s32 digit;
    s32 i = 0;

    sprite->w = 8;
    y = sprite->y;
    sprite->h = 0x10;
    for (; i < digits - 1; i++) {
        divisor *= 10;
    }

    for (i = 0; i < digits; i++) {
        if (value >= divisor || i == digits - 1) {
            digit = value / divisor;
            digit = digit % 10;
            sprite->cx = 0x70;
            sprite->u = digit * 8 - 0x58;
            if (digit < 6) {
                sprite->v = 0x34;
                sprite->h = 0xC;
                sprite->y += 4;
            } else {
                sprite->v = 0x31;
                sprite->h = 0xF;
                sprite->y += 1;
            }
            GsSortFastSprite(sprite, ot, g_hud_ot_priority);
            sprite->y = y;
        } else {
            if (zero_pad != 0) {
                sprite->u = 0xA8;
                sprite->cx = 0x70;
                sprite->v = 0x31;
                sprite->h = 0xF;
                sprite->y += 1;
            } else {
                sprite->u = 0xD0;
                sprite->v = 0x28;
                sprite->cx = 0x70;
                sprite->h = 8;
                sprite->y += 4;
            }
            GsSortFastSprite(sprite, ot, g_hud_ot_priority);
            sprite->y = y;
        }
        sprite->x += 8;
        divisor /= 10;
    }
}

/* Draws the world map's date: the month's zodiac sequence for the month
 * script variable, then the day as one or two digit sequences 0x1A pixels to
 * its right, in the tinted HUD color. Nothing is drawn while
 * SYSTEM_FLAG_HIDE_CALENDAR_DATE is set. */
void calendar_date_draw(GsOT* ot) {
    display_object_animated_t request;
    CVECTOR color;
    s32 first;
    s32 second;

    if (g_system_flags & SYSTEM_FLAG_HIDE_CALENDAR_DATE) {
        return;
    }

    request.flags = DISPLAY_OBJECT_ANIMATED;
    request.priority = 4;
    request.palette = 0;
    request.anim_counter = 0;
    request.frame_index = 0;
    request.position.x = g_calendar_display_position.x;
    request.position.y = g_calendar_display_position.y;
    world_map_fade_tint_color(&g_hud_color, &color);
    request.color.r = color.r;
    request.color.g = color.g;
    request.color.b = color.b;

    first = script_variable_get(EVENT_SCRIPT_VAR_MONTH);
    second = script_variable_get(EVENT_SCRIPT_VAR_DAY);
    request.sequence = first + (WINDOW_ANIM_MONTH_FIRST - 1);
    window_gs_animated_record_draw(&request, ot);

    request.position.x += 0x1A;
    request.position.y -= 4;
    if (second >= 10) {
        request.anim_counter = 0;
        request.frame_index = 0;
        request.sequence = second / 10 + WINDOW_ANIM_DIGIT_FIRST;
        window_gs_animated_record_draw(&request, ot);
        request.position.x += 8;
    }
    request.sequence = second % 10 + WINDOW_ANIM_DIGIT_FIRST;
    window_gs_animated_record_draw(&request, ot);
}

/* Draws the name label of the selected location (g_location_selected,
 * location + 1) over its map dot, unless SYSTEM_FLAG_HIDE_LOCATION_LABEL
 * is set or no location is selected.
 *
 * The color is g_location_label_shade replicated across the three
 * channels and tinted in place by world_map_fade_tint_color. sequence
 * is written after that call because the target reloads the selection
 * there. */
void location_label_draw(GsOT* ot) {
    display_object_animated_t request;
    CVECTOR color;
    s32 index;
    s32 shade;

    if (g_system_flags & SYSTEM_FLAG_HIDE_LOCATION_LABEL) {
        return;
    }
    if (g_location_selected == 0) {
        return;
    }

    index = g_location_selected - 1;
    request.flags = DISPLAY_OBJECT_ANIMATED;
    request.position.x = g_world_map_dots[index].render.position.x;
    request.position.y = g_world_map_dots[index].render.position.y;
    shade = g_location_label_shade;
    request.priority = 4;
    request.palette = 0;
    request.anim_counter = 0;
    request.frame_index = 0;
    color.r = shade;
    color.g = shade;
    color.b = shade;
    world_map_fade_tint_color(&color, &color);
    request.color.r = color.r;
    request.color.g = color.g;
    request.color.b = color.b;
    request.sequence = g_location_selected + (WINDOW_ANIM_LOCATION_NAME_FIRST - 1);
    window_gs_animated_record_draw(&request, ot);
}
