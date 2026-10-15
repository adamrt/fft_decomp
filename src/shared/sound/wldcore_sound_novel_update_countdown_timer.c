#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/types.h"

/* Same 32-bit value as g_wldcore_sound_novel_countdown_frames; the target stores 0 through
 * both spellings, and a single name lets GCC merge the two stores. */
extern s32 g_wldcore_sound_novel_countdown_frames_reset;

/*
 * Steps the sound novel's choice countdown. Returns 1 on the frame the script
 * is redirected to the timeout section, 0 otherwise.
 *
 * While a countdown is set, each call counts one frame and, with the play
 * time display selected, shows the time remaining (60 frames to the second).
 * When it runs out the message thread is told to close (task 3) and the
 * countdown becomes -1; once that thread has ended the script branches to
 * g_wldcore_sound_novel_countdown_section.
 */
s32 wldcore_sound_novel_update_countdown_timer(void) {
    s32 elapsed;
    s32 remaining;
    s32 hours;
    s32 days;

    if (g_wldcore_sound_novel_countdown_frames == -1) {
        if (thread_is_running(WLDCORE_THREAD_SLOT_MESSAGE) != 0) {
            return 0;
        }
        g_wldcore_sound_novel_countdown_frames = 0;
        g_wldcore_sound_novel_countdown_frames_reset = 0;
        g_wldcore_active_saved_record.section = g_wldcore_sound_novel_countdown_section;
        g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_BRANCH;
        return 1;
    }

    if (g_wldcore_sound_novel_countdown_frames != 0) {
        elapsed = g_wldcore_sound_novel_countdown_elapsed + 1;
        g_wldcore_sound_novel_countdown_elapsed = elapsed;
        remaining = g_wldcore_sound_novel_countdown_frames - elapsed;
        if (g_wldcore_context_value_display_mode == WLDCORE_VALUE_DISPLAY_PLAY_TIME) {
            days = remaining / 216000;
            hours = remaining / 3600;
            g_wldcore_displayed_numeric_value_secondary = hours - days * 60;
            g_wldcore_displayed_numeric_value_tertiary = remaining / 60 - hours * 60;
            g_wldcore_displayed_numeric_value = days % 24;
        }
        if (g_wldcore_sound_novel_countdown_frames == elapsed) {
            world_thread_request_stop(WLDCORE_THREAD_SLOT_MESSAGE);
            g_wldcore_sound_novel_countdown_frames = -1;
        }
    }
    return 0;
}
