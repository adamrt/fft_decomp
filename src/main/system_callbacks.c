/* VSync play-time accounting and the installed drawing and CD callback hooks. */

#include "fft/file.h"
#include "fft/save.h"
#include "fft/system.h"
#include "psx/libc.h"
#include "psx/types.h"

/* VSync callback. Draws one rand() per frame, advances the play-time clock at 60
 * frames a second (the hours stop at 1000) and counts
 * g_system_wait_frames. */
void system_handle_vsync(void) {
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

/* Empty DrawSync callback, installed by boot_run_startup. */
void system_handle_draw_sync(void) {
}

/* Empty CdReadyCallback handler, installed by boot_run_startup; the CdInit
 * that file_reset_cdrom_cpu_ram runs right after replaces it, so it never
 * runs. */
void file_handle_cd_ready(u8 event, u8* result) {
}

/* Empty CdReadCallback handler, installed by boot_run_startup and replaced
 * by CdInit right after, like the ready callback. */
void file_handle_cd_read(u8 event, u8* result) {
}
