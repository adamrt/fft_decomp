/* Prepare the new-game roster after loading WORLD and convert the chosen
 * birthday into Ramza's zodiac sign and day within that sign. */

#include "fft/calendar.h"
#include "fft/formation.h"
#include "fft/overlay.h"
#include "fft/party.h"
#include "fft/system.h"
#include "fft/title.h"
#include "psx/types.h"

/* Load WORLD if needed, then prepare Ramza's roster or the empty Continue
 * roster. Remember the party mode so repeated calls do not rebuild it. */
void new_game_party_init(s32 party_mode, s32 world_load_mode) {
    u32 flags;
    u32 party_mode_flag;

    if ((g_system_runtime_flags & SYSTEM_RUNTIME_FLAG_WORLD_OVERLAY_LOADED) == 0) {
        g_system_runtime_flags |= SYSTEM_RUNTIME_FLAG_WORLD_OVERLAY_LOADED;
        overlay_world_load(world_load_mode);
    }

    party_mode_flag = NEW_GAME_PARTY_MODE_FLAG_BASE << party_mode;
    flags = g_system_runtime_flags;
    if ((flags & SYSTEM_RUNTIME_FLAG_PARTY_CHOSEN) == 0 || (flags & party_mode_flag) == 0) {
        g_system_runtime_flags
            = ((flags | SYSTEM_RUNTIME_FLAG_PARTY_CHOSEN) & ~NEW_GAME_PARTY_MODE_FLAG_MASK) | party_mode_flag;
        party_new_game_init(party_mode);
        formation_roster_names_build();
    }
}

/* Start date of each ordinary sign as {month, day} byte pairs, Aries first.
 * calendar_date_to_zodiac_convert indexes it flat ([i * 2], [i * 2 + 1]),
 * and it is not const: that routine reloads a day after storing through its
 * month pointer. */
static u8 s_birthday_zodiac_start_dates[ZODIAC_SIGN_ORDINARY_COUNT * 2] = {
    3,
    21,
    4,
    20,
    5,
    21,
    6,
    22,
    7,
    23,
    8,
    23,
    9,
    23,
    10,
    24,
    11,
    23,
    12,
    23,
    1,
    20,
    2,
    19,
};

/* Convert a calendar month/day into a zodiac sign number (1 = Aries) and the
 * 1-based day within that sign.
 *
 * `span` first holds the previous sign's start day and then the number of
 * days from it to the end of its month; computing it in two steps keeps GCC
 * from folding the `+ 1` into the sum with `*day`. */
void calendar_date_to_zodiac_convert(s32* month, s32* day) {
    s32 index = 0;
    s32 length;
    s32 span;

    while (s_birthday_zodiac_start_dates[index * 2] != *month) {
        index = (index + 1) % ZODIAC_SIGN_ORDINARY_COUNT;
    }
    if (*day < s_birthday_zodiac_start_dates[index * 2 + 1]) {
        index = (index + ZODIAC_SIGN_ORDINARY_COUNT - 1) % ZODIAC_SIGN_ORDINARY_COUNT;
        *month = index + 1;
        length = g_calendar_month_lengths[s_birthday_zodiac_start_dates[index * 2] - 1];
        span = s_birthday_zodiac_start_dates[index * 2 + 1];
        span = length - span + 1;
        *day += span;
        return;
    }
    *month = index + 1;
    *day = *day - s_birthday_zodiac_start_dates[index * 2 + 1] + 1;
}
