/* VSync play-time accounting and the installed drawing and CD callback hooks. */

#include "fft/file.h"
#include "fft/save.h"
#include "fft/system.h"
#include "psx/libc.h"
#include "psx/types.h"

s32 g_save_play_time_frames;
s32 g_save_play_time_seconds;
s32 g_save_play_time_minutes;
s32 g_save_play_time_hours;

/* VSync callback. Draws one rand() per frame, advances the play-time clock at 60
 * frames a second (the hours stop at 1000) and counts
 * g_system_wait_frames. */
void system_vsync_callback(void) {
    rand();

    g_save_play_time_frames++;
    if (g_save_play_time_frames >= 60) {
        g_save_play_time_frames = 0;
        g_save_play_time_seconds++;
        if (g_save_play_time_seconds >= 60) {
            g_save_play_time_seconds = 0;
            g_save_play_time_minutes++;
            if (g_save_play_time_minutes >= 60) {
                g_save_play_time_minutes = 0;
                if (g_save_play_time_hours < 1000) {
                    g_save_play_time_hours++;
                }
            }
        }
    }

    g_system_wait_frames++;
}

/* Empty DrawSync callback, installed by boot_startup_run. */
void system_draw_sync_callback(void) {
}

/* Empty CdReadyCallback handler, installed by boot_startup_run; the CdInit
 * that cd_memory_init runs right after replaces it, so it never
 * runs. */
void file_cd_ready_callback(u8 event, u8* result) {
}

/* Empty CdReadCallback handler, installed by boot_startup_run and replaced
 * by CdInit right after, like the ready callback. */
void file_cd_read_callback(u8 event, u8* result) {
}
