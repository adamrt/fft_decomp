#include "fft/main_runtime.h"
#include "fft/wldcore.h"
#include "psx/types.h"

/*
 * Copies a two-word state snapshot, sets its control flags, and captures the
 * current play time.
 *
 * Cache the flag and time source values before publishing their destinations;
 * this preserves the target's load/store schedule.
 */
void wldcore_capture_state_and_play_time(void) {
    u32 flags;
    s32 hours;
    s32 minutes;
    s32 seconds;

    g_wldcore_context_value_display_position = g_wldcore_saved_context_value_display_position;
    g_wldcore_context_value_display_mode = 2;
    /* The target stores the whole word (sw), not the u16 the sorts read. */
    *(s32*)&g_wldcore_hud_ot_priority = 1;
    flags = g_main_system_flags;
    hours = g_main_system_play_time_hours;
    minutes = g_main_system_play_time_minutes;
    seconds = g_main_system_play_time_seconds;
    g_main_system_flags = flags | 0x800;
    g_wldcore_displayed_numeric_value = hours;
    g_wldcore_displayed_numeric_value_secondary = minutes;
    g_wldcore_displayed_numeric_value_tertiary = seconds;
}
