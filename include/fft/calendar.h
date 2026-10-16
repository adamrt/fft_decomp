#ifndef CALENDAR_H
#define CALENDAR_H

/* The in-game date, the days passing, zodiac signs from birthdays and the yearly birthday updates; OPEN's birthday
 * screen shares its date-to-zodiac routine. */

#include "fft/unit.h"
#include "psx/libgpu.h"
#include "psx/libgs.h"
#include "psx/libgte.h"
#include "psx/suzuki.h"
#include "psx/types.h"

/* Calendar month and day, stored as separate bytes. */
typedef struct calendar_date {
    u8 month;
    u8 day;
} calendar_date_t;
typedef char calendar_date_size_must_be_2[sizeof(calendar_date_t) == 2 ? 1 : -1];

/* days */
extern u8 g_calendar_month_lengths[12];
extern u16 g_calendar_month_start_day_offsets[13];

s32 calendar_calculate_day_of_year(s32 month, s32 day);
void calendar_calculate_month_and_day(s32 day_of_year, s32* output_month, s32* output_day);
void calendar_advance_day(void);
void calendar_advance_days(s32 days);
void calendar_advance_one_year(void);

/* zodiac signs */
/* Twelve {month, day} pairs: the calendar date each zodiac sign starts on,
 * Aries (3/21) first. */
extern calendar_date_t g_calendar_zodiac_start_dates[ZODIAC_SIGN_ORDINARY_COUNT];

void calendar_convert_date_to_zodiac(s32* month, s32* day);

/* date bitmaps */
/* Dates kept as nine-bit records in a bit array (day in bits 0-4, month in bits 5-8), as the save's acquisition and
 * discovery date bitmaps hold them. */
void calendar_write_date(u32* bits, s32 record, calendar_date_t* source);
void calendar_read_date(u32* bits, s32 record, calendar_date_t* dest);

/* date display */
/* Screen position of the date calendar_draw_date draws. */
extern s32 g_calendar_display_x;
extern s32 g_calendar_display_y;

void calendar_draw_date(GsOT* ot);

#endif
