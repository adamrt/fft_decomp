/* World-menu visibility and screen fades, proposition data and calendar updates. */

#include "fft/bits.h"
#include "fft/brave_story.h"
#include "fft/calendar.h"
#include "fft/formation.h"
#include "fft/gfx.h"
#include "fft/location.h"
#include "fft/music.h"
#include "fft/party.h"
#include "fft/proposition.h"
#include "fft/screen.h"
#include "fft/script_variable.h"
#include "fft/sound.h"
#include "fft/system.h"
#include "fft/unit_party.h"
#include "fft/window.h"
#include "fft/world_map.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/libgs.h"
#include "psx/types.h"

/* Twelve {month, day} pairs, Aries first. */
extern calendar_date_t g_calendar_zodiac_start_dates[ZODIAC_SIGN_ORDINARY_COUNT];
extern s32* g_proposition_data; /* byte-offset directory of the embedded proposition data */

static void calendar_advance_birthdays(void);

/* Clear (nonzero argument) or set the blank flag on both display-object queues.
 * Both record types start with the same signed 32-bit flags field. */
void window_set_render_objects_visible(s32 clear) {
    s32 i;

    if (clear) {
        for (i = 0; i < g_window_aux_render_object_count; i++) {
            *(s32*)g_window_aux_render_object_queue[i] &= ~DISPLAY_OBJECT_BLANK;
        }
        for (i = 0; i < g_window_render_object_count; i++) {
            *(s32*)g_window_render_object_queue[i] &= ~DISPLAY_OBJECT_BLANK;
        }
    } else {
        for (i = 0; i < g_window_aux_render_object_count; i++) {
            *(s32*)g_window_aux_render_object_queue[i] |= DISPLAY_OBJECT_BLANK;
        }
        for (i = 0; i < g_window_render_object_count; i++) {
            *(s32*)g_window_render_object_queue[i] |= DISPLAY_OBJECT_BLANK;
        }
    }
}

/* Start a screen fade of `duration` frames unless the current fade already has the same direction
 * (WORLD_MAP_FADE_OUT bit); START_MUSIC also queues the world map theme with a fade-in,
 * STOP_MUSIC fades the music out and unloads it. */
void world_map_fade_start(s32 flags, s32 duration) {
    if ((g_world_map_fade.flags[0] & WORLD_MAP_FADE_OUT) != (flags & WORLD_MAP_FADE_OUT)) {
        g_world_map_fade.flags[0] = flags | WORLD_MAP_FADE_RUNNING;
        g_world_map_fade.elapsed = 0;
        g_world_map_fade.duration = duration;
        g_system_flags |= SYSTEM_FLAG_FADING;
        if (flags & WORLD_MAP_FADE_START_MUSIC) {
            music_queue_add(MUSIC_QUEUE_COMMAND_PLAY_TRACK, MUSIC_QUEUE_TRACK(1, MUSIC_WORLD_MAP));
            music_queue_add(MUSIC_QUEUE_COMMAND_FADE_IN, 0x10);
        }
        if (flags & WORLD_MAP_FADE_STOP_MUSIC) {
            music_queue_add(MUSIC_QUEUE_COMMAND_FADE_OUT, 4);
            music_queue_add(MUSIC_QUEUE_COMMAND_UNLOAD, 2);
        }
    }
}

/* Steps the world screen fade: ramps the menu brightness and the first fill box
 * color toward the target until the elapsed counter reaches the duration. */
s32 world_map_fade_step(void) {
    u32 flags;
    s32 elapsed;
    s32 brightness;
    s32 box_level;

    flags = g_world_map_fade.flags[0];
    if (flags & WORLD_MAP_FADE_RUNNING) {
        if (g_world_map_fade.elapsed == g_world_map_fade.duration) {
            g_world_map_fade.flags[0]
                = flags & ~(WORLD_MAP_FADE_RUNNING | WORLD_MAP_FADE_REDRAW_MAP | WORLD_MAP_FADE_BOX_VISIBLE);
        } else {
            if (g_world_map_fade.elapsed + 1 == g_world_map_fade.duration) {
                g_system_flags &= ~SYSTEM_FLAG_FADING;
                music_queue_wait_until_empty();
            }
            if (g_world_map_fade.flags[0] & WORLD_MAP_FADE_OUT) {
                elapsed = g_world_map_fade.elapsed;
                brightness = 0x70 - (elapsed << 7) / g_world_map_fade.duration;
                box_level = (elapsed << 8) / g_world_map_fade.duration + 0x20;
                if (brightness < 0) {
                    brightness = 0;
                }
                if (box_level >= 0x100) {
                    box_level = 0xFF;
                }
                if (elapsed < 0) {
                    brightness = 0x80;
                    box_level = 0;
                }
            } else {
                elapsed = g_world_map_fade.elapsed;
                brightness = (elapsed << 7) / g_world_map_fade.duration + 0x10;
                box_level = 0xC0 - (elapsed << 8) / g_world_map_fade.duration;
                if (brightness > 0x80) {
                    brightness = 0x80;
                }
                if (box_level < 0) {
                    box_level = 0;
                }
                if (elapsed < 0) {
                    brightness = 0;
                    box_level = 0x80;
                }
            }
            g_world_map_fade.elapsed++;
            screen_set_brightness(brightness, brightness, brightness);
            g_world_map_brightness_rgb.r = brightness;
            g_world_map_brightness_rgb.g = brightness;
            g_world_map_brightness_rgb.b = brightness;
            g_world_map_fade.boxes[0].r = box_level;
            g_world_map_fade.boxes[0].g = box_level;
            g_world_map_fade.boxes[0].b = box_level;
            if (g_world_map_fade.flags[0] & WORLD_MAP_FADE_REDRAW_MAP) {
                g_system_flags |= SYSTEM_FLAG_DRAW_MAP;
            }
            return 1;
        }
    }

    return 0;
}

/* Copies a packed 24-bit RGB triple, applying the global per-channel tint
 * when bit 0 of the 32-bit fade state is set. Each channel is scaled by
 * ((c << 16) / 128) * factor >> 16, i.e. c * factor / 128, and clamped to
 * 255 with no lower clamp; with the factors at their 0x80 default the scale
 * is the identity. When the bit is clear the triple is copied verbatim,
 * which the 4-byte alignment-1 struct assignment emits as the unaligned
 * lwl/lwr + swl/swr quartet. */
void world_map_fade_tint_color(CVECTOR* source, CVECTOR* out) {
    s32 red;
    s32 green;
    s32 blue;

    if (g_world_map_fade.flags[0] & WORLD_MAP_FADE_RUNNING) {
        red = ((source->r << 16) / 128 * g_world_map_brightness_rgb.r) >> 16;
        green = ((source->g << 16) / 128 * g_world_map_brightness_rgb.g) >> 16;
        blue = ((source->b << 16) / 128 * g_world_map_brightness_rgb.b) >> 16;
        if (red >= 256) {
            red = 255;
        }
        if (green >= 256) {
            green = 255;
        }
        if (blue >= 256) {
            blue = 255;
        }
        out->r = red;
        out->g = green;
        out->b = blue;
    } else {
        *out = *source;
    }
}

/* Sort the two screen-fade boxes into the active ordering table at their
 * priorities. A box is drawn while its fade runs or it is flagged visible,
 * and only when its color is not black. */
void world_map_fade_draw_boxes(void) {
    u32* flags;
    GsBOXF* boxes;
    s32* priorities;
    s32 i;

    flags = g_world_map_fade.flags;
    boxes = g_world_map_fade.boxes;
    for (i = 0; i < 2; i++) {
        /* Reloaded inside the loop so that the priority walk, not its invariant
         * copy, holds the register the exit test compares. */
        priorities = g_world_map_fade.priorities;
        if (flags[i] & (WORLD_MAP_FADE_RUNNING | WORLD_MAP_FADE_BOX_VISIBLE)) {
            if (g_world_map_fade.boxes[i].r != 0 || g_world_map_fade.boxes[i].g != 0
                || g_world_map_fade.boxes[i].b != 0) {
                GsSortBoxFill(&boxes[i], &g_world_map_ordering_tables[g_gfx_active_buffer_index], priorities[i]);
            }
        }
    }
}

/* Return the address of proposition data record `record`: the data block starts with a table of
 * byte offsets from its own start. */
void* proposition_get_data(proposition_data_record_e record) {
    return (u8*)g_proposition_data + g_proposition_data[record];
}

/* Expand one 23-byte proposition row into 16-bit fields, then remap the
 * two fee fields through the 16-bit table of record 1. */
void proposition_unpack_row(proposition_fields_t* out, s32 index) {
    u8* rows = proposition_get_data(PROPOSITION_DATA_ROWS);
    u16* field_lookup;
    s32 i;
    u16* fields = &out->category_index;

    for (i = 0; i < 23; i++) {
        fields[i] = rows[index * 23 + i];
    }
    field_lookup = proposition_get_data(PROPOSITION_DATA_FEES);
    out->fee_a = field_lookup[out->fee_a];
    out->fee_b = field_lookup[out->fee_b];
}

/* Whether proposition `index` is offered at `location`: the row's location
 * (0 for any), then each condition its flags select: a shop stock level, the
 * current zodiac sign, or another proposition (condition_value) completed. */
s32 proposition_is_available_at(s32 index, s32 location) {
    proposition_fields_t entry;
    s32 month;
    s32 day;

    proposition_unpack_row(&entry, index);
    if (entry.location != 0) {
        if (location != entry.location) {
            return 0;
        }
    }
    if (entry.condition_flags & PROPOSITION_CONDITION_SHOP_AVAILABILITY) {
        if (script_get_variable(EVENT_SCRIPT_VAR_SHOP_ITEM_AVAILABILITY) < entry.required_shop_availability) {
            return 0;
        }
    }
    if (entry.condition_flags & PROPOSITION_CONDITION_ZODIAC_SIGN) {
        month = script_get_variable(EVENT_SCRIPT_VAR_MONTH);
        day = script_get_variable(EVENT_SCRIPT_VAR_DAY);
        calendar_convert_date_to_zodiac(&month, &day);
        if (month != entry.condition_value) {
            return 0;
        }
    }
    if (entry.condition_flags & PROPOSITION_CONDITION_STATUS) {
        if ((script_get_variable(entry.condition_value + EVENT_SCRIPT_VAR_PROPOSITION_FIRST)
                & PROPOSITION_VARIABLE_COMPLETED)
            == 0) {
            return 0;
        }
    }
    return 1;
}

/*
 * Advance the calendar by 1 day and run the daily updates.
 *
 * EVENT_SCRIPT_VAR_UNKNOWN_67 increases on 21 March (the start of Aries) and
 * is capped at 99; calendar_advance_days raises it at the year's end
 * instead, without the cap. The target does not validate the incoming month
 * or day.
 */
void calendar_advance_day(void) {
    s32 month = script_get_variable(EVENT_SCRIPT_VAR_MONTH);
    s32 day = script_get_variable(EVENT_SCRIPT_VAR_DAY) + 1;

    if (g_calendar_month_lengths[month - 1] < day) {
        month++;
        if (month == 13) {
            month = 1;
        }
        day = 1;
    }
    script_set_variable(EVENT_SCRIPT_VAR_MONTH, month);
    script_set_variable(EVENT_SCRIPT_VAR_DAY, day);
    if (month == 3 && day == 21) {
        s32 value = script_get_variable(EVENT_SCRIPT_VAR_UNKNOWN_67) + 1;
        if (value >= 100) {
            value = 99;
        }
        script_set_variable(EVENT_SCRIPT_VAR_UNKNOWN_67, value);
    }
    proposition_advance_day();
    calendar_advance_birthdays();
    breeding_update(1);
}

/* Converts a calendar date in place into a zodiac sign number (1 = Aries) and
 * the day within that sign. An invalid date plays SOUND_SFX_INVALID and
 * becomes 1/1. */
void calendar_convert_date_to_zodiac(s32* month_ptr, s32* day_ptr) {
    s32 index = 0;
    s32 delta;
    s32 previous_month;
    calendar_date_t* start_dates;
    u8* start_days;
    calendar_date_t* previous_dates;

    if (*month_ptr < 1 || *month_ptr > 12 || *day_ptr < 1 || *day_ptr > 31) {
        sound_play_effect(SOUND_SFX_INVALID);
        *day_ptr = 1;
        *month_ptr = 1;
        return;
    }
    start_dates = g_calendar_zodiac_start_dates;
    while (start_dates[index].month != *month_ptr) {
        index = (index + 1) % 12;
    }
    /* Direct .day indexing folds the two original indexed addresses into one. */
    start_days = &g_calendar_zodiac_start_dates[0].day;
    if (*day_ptr < start_days[index * sizeof(calendar_date_t)]) {
        index = (index + 11) % 12;
        *month_ptr = index + 1;
        /* Separate month-table local preserves the original register allocation. */
        previous_dates = g_calendar_zodiac_start_dates;
        previous_month = previous_dates[index].month;
        delta = start_days[index * sizeof(calendar_date_t)];
        delta = g_calendar_month_lengths[previous_month - 1] - delta + 1;
        *day_ptr += delta;
    } else {
        *month_ptr = index + 1;
        *day_ptr = *day_ptr - start_days[index * sizeof(calendar_date_t)] + 1;
    }
}

/*
 * The daily birthday sweep, run by calendar_advance_day after the
 * calendar rolls over: every brave-story character whose stored birthday is
 * today gets a year older, capped at 99.
 *
 * Entry 0 is the player character. Its row in the birthday table is a
 * placeholder (1/1); the real birthday lives in
 * EVENT_SCRIPT_VAR_PLAYER_BIRTH_MONTH and _DAY, written when the player picks
 * one at game start, so entry 0 is compared against those two variables and
 * the remaining 63 entries against the table. The table contains 64 calendar dates
 * (g_brave_story_birthday_table), reached through
 * the pointer g_brave_story_birthdays.
 *
 * The age column is walked one byte at a time from index 1, which is why the
 * loop's base materializes as g_brave_story_character_ages + 1; entry 0
 * is the only absolute access to the array.
 */
static void calendar_advance_birthdays(void) {
    const calendar_date_t* birthdays = g_brave_story_birthdays;
    s32 day = script_get_variable(EVENT_SCRIPT_VAR_DAY);
    s32 month = script_get_variable(EVENT_SCRIPT_VAR_MONTH);
    s32 birth_month = script_get_variable(EVENT_SCRIPT_VAR_PLAYER_BIRTH_MONTH);
    s32 birth_day = script_get_variable(EVENT_SCRIPT_VAR_PLAYER_BIRTH_DAY);
    u8 age;
    s32 i;

    if (month == birth_month && day == birth_day) {
        age = g_brave_story_character_ages[0];
        if (age < 99) {
            g_brave_story_character_ages[0] = age + 1;
        }
    }

    for (i = 1; i < 64; i++) {
        if (month == birthdays[i].month && day == birthdays[i].day) {
            if (g_brave_story_character_ages[i] < 99) {
                g_brave_story_character_ages[i]++;
            }
        }
    }
}

/* WLDCORE's wrapper for party_get_unit: the roster record of slot `party_index`. */
party_unit_t* party_get_roster_unit(s32 party_index) {
    return party_get_unit(party_index);
}

/* Writes one nine-bit record: bits 0-4 from source->day, bits 5-8 from
 * source->month. Records are packed nine bits apart in the bit array. */
void calendar_write_date(u32* bits, s32 record, calendar_date_t* source) {
    s32 base = record * 9;
    s32 mask;
    s32 i;

    mask = 1;
    for (i = 0; i < 5; i++) {
        bits_set_value(bits, base + i, source->day & mask);
        mask <<= 1;
    }
    mask = 1;
    for (i = 0; i < 4; i++) {
        /* The separate temporary keeps the target's add order: folding
         * base + (i + 5) in one expression hoists base + 5 out of the loop. */
        s32 index = i + 5;

        bits_set_value(bits, base + index, source->month & mask);
        mask <<= 1;
    }
}

/* Reads one nine-bit record: bits 0-4 into dest->day, bits 5-8 into dest->month.
 * Records are packed nine bits apart in the bit array, matching
 * calendar_write_date. A field that decodes to zero or beyond its
 * range falls back to 1. */
void calendar_read_date(u32* bits, s32 record, calendar_date_t* dest) {
    s32 base = record * 9;
    s32 mask;
    s32 i;

    mask = 1;
    dest->day = 0;
    for (i = 0; i < 5; i++) {
        if (bits_test(bits, base + i)) {
            dest->day |= mask;
        }
        mask <<= 1;
    }
    mask = 1;
    dest->month = 0;
    for (i = 0; i < 4; i++) {
        s32 index = i + 5;

        if (bits_test(bits, base + index)) {
            dest->month |= mask;
        }
        mask <<= 1;
    }
    if (dest->day == 0 || dest->day >= 32) {
        dest->day = 1;
    }
    if (dest->month == 0 || dest->month >= 13) {
        dest->month = 1;
    }
}

/* Clear both frame buffers (256 x 480 at the VRAM origin) to black and, when
 * asked, blank the 256x240 backdrop image in g_world_map_scratch_buffer. */
void world_map_clear_vram_and_scratch(s32 clear_scratch) {
    RECT image_rect;
    s32 i;

    DrawSync(0);
    VSync(0);
    image_rect.x = 0;
    image_rect.y = 0;
    image_rect.w = 0x100;
    image_rect.h = 0x1e0;
    ClearImage(&image_rect, 0, 0, 0);
    if (clear_scratch != 0) {
        for (i = WORLD_MAP_BACKDROP_WORDS - 1; i >= 0; i--) {
            g_world_map_scratch_buffer[i] = 0;
        }
    }
    DrawSync(0);
}
