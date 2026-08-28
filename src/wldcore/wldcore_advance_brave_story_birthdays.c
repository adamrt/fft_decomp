#include "fft/main_runtime.h"
#include "fft/script_variables.h"
#include "fft/wldcore.h"
#include "fft/world.h"

/*
 * The daily birthday sweep, run by wldcore_advance_calendar_day after the
 * calendar rolls over: every brave-story character whose stored birthday is
 * today gets a year older, capped at 99.
 *
 * Entry 0 is the player character. Its row in the birthday table is a
 * placeholder (1/1); the real birthday lives in script variables 0x5f (month)
 * and 0x60 (day), written when the player picks one at game start, so entry 0
 * is compared against those two variables and the remaining 63 entries
 * against the table. The table is a 64 x 2 array of month/day byte pairs
 * reached through the pointer cell at 0x80096a48 (it points at 0x80097028,
 * whose 128 bytes read as plausible month/day pairs throughout).
 *
 * The age column is walked one byte at a time from index 1, which is why the
 * loop's base materialises as the biased constant 0x80057f35; entry 0 is the
 * only absolute access to 0x80057f34.
 *
 * Both empty barriers are load-bearing:
 *   - the one inside the age-cap block keeps the loop's `i = 1` in the
 *     preheader, so reorg copies it into the month compare's delay slot
 *     (the target's duplicate `li a2,1`) instead of moving it there;
 *   - the one at the end of the loop body keeps `i++` at the bottom of the
 *     body instead of in the first birthday compare's delay slot, which the
 *     target leaves as a nop.
 */
void wldcore_advance_brave_story_birthdays(void) {
    const u8(*birthdays)[2] = g_wldcore_brave_story_birthdays;
    s32 day = world_script_get_variable(EVENT_SCRIPT_VAR_DAY);
    s32 month = world_script_get_variable(EVENT_SCRIPT_VAR_MONTH);
    s32 birth_month = world_script_get_variable(EVENT_SCRIPT_VAR_PLAYER_BIRTH_MONTH);
    s32 birth_day = world_script_get_variable(EVENT_SCRIPT_VAR_PLAYER_BIRTH_DAY);
    u8 age;
    s32 i;

    if (month == birth_month && day == birth_day) {
        age = g_main_brave_story_character_ages[0];
        if (age < 99) {
            g_main_brave_story_character_ages[0] = age + 1;
            /* Keeps `i = 1` in the loop preheader (see above). */
            __asm__ volatile("");
        }
    }

    for (i = 1; i < 64; i++) {
        if (month == birthdays[i][0] && day == birthdays[i][1]) {
            if (g_main_brave_story_character_ages[i] < 99) {
                g_main_brave_story_character_ages[i]++;
            }
        }
        /* Keeps `i++` out of the compare's delay slot (see above). */
        __asm__ volatile("");
    }
}
