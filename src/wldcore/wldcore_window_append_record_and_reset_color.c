#include "fft/wldcore.h"
#include "psx/types.h"

/* Attach the current window record to the caller's list and reset its color. */
s32 wldcore_window_append_record_and_reset_color(void** queue, s32* count) {
    CVECTOR* color;

    queue[*count] = &g_wldcore_window_records[g_wldcore_window_record_count];
    /* Direct record.color access changes the column base and instruction scheduling. */
    color = &g_wldcore_window_records->color;
    g_wldcore_window_records[g_wldcore_window_record_count].palette = 0;
    g_wldcore_window_records[g_wldcore_window_record_count].flags
        &= ~(WORLD_RENDER_RECORD_BLANK | WORLD_RENDER_RECORD_HIDDEN | WORLD_RENDER_RECORD_UNTINTED);
    ((CVECTOR*)((u8*)color + g_wldcore_window_record_count * sizeof(g_wldcore_window_records[0])))->r = 0x80;
    ((CVECTOR*)((u8*)color + g_wldcore_window_record_count * sizeof(g_wldcore_window_records[0])))->g = 0x80;
    ((CVECTOR*)((u8*)color + g_wldcore_window_record_count * sizeof(g_wldcore_window_records[0])))->b = 0x80;
    *count += 1;
    return g_wldcore_window_record_count++;
}
