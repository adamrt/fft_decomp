#ifndef CALENDAR_H
#define CALENDAR_H

/* The in-game date, the days passing, zodiac signs from birthdays and the yearly birthday updates; OPEN's birthday
 * screen shares its date-to-zodiac routine. */

#include "fft/gfx.h"
#include "psx/libgs.h"
#include "psx/types.h"

/* Calendar month and day, stored as separate bytes. */
typedef struct calendar_date {
    u8 month;
    u8 day;
} calendar_date_t;
typedef char calendar_date_size_must_be_2[sizeof(calendar_date_t) == 2 ? 1 : -1];

/* days */
extern u8 g_calendar_month_lengths[12];

s32 calendar_date_to_day_index(s32 month, s32 day);
void calendar_day_index_to_date(s32 day_of_year, s32* output_month, s32* output_day);
void calendar_day_advance(void);
void calendar_days_advance(s32 days);
void calendar_year_advance(void);

/* zodiac signs */
void calendar_date_to_zodiac_convert(s32* month, s32* day);

/* date bitmaps */
/* Dates kept as nine-bit records in a bit array (day in bits 0-4, month in bits 5-8), as the save's acquisition and
 * discovery date bitmaps hold them. */
void calendar_date_write(u32* bits, s32 record, calendar_date_t* source);
void calendar_date_read(u32* bits, s32 record, calendar_date_t* dest);

/* date display */
/* Screen position of the date calendar_date_draw draws. */
extern point32_t g_calendar_display_position;

void calendar_date_draw(GsOT* ot);

#endif
