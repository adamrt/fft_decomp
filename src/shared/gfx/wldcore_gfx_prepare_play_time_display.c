#include "fft/wldcore.h"
#include "psx/types.h"

/* Restore the HUD position and display the current play time.
 * Cache the flags and time before publishing the display fields. */
void wldcore_gfx_prepare_play_time_display(void) {
    u32 flags;
    s32 hours;
    s32 minutes;
    s32 seconds;

    g_wldcore_context_value_display_position = g_wldcore_saved_context_value_display_position;
    g_wldcore_context_value_display_mode = WLDCORE_VALUE_DISPLAY_PLAY_TIME;
    g_wldcore_hud_ot_priority = 1;
    flags = g_main_system_flags;
    hours = g_main_system_play_time_hours;
    minutes = g_main_system_play_time_minutes;
    seconds = g_main_system_play_time_seconds;
    g_main_system_flags = flags | MAIN_SYSTEM_FLAG_HIDE_CONTEXT_VALUE;
    g_wldcore_displayed_numeric_value = hours;
    g_wldcore_displayed_numeric_value_secondary = minutes;
    g_wldcore_displayed_numeric_value_tertiary = seconds;
}
