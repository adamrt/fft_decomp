#include "fft/wldcore.h"
#include "psx/types.h"

/* Attach the current window record to the caller's list and reset its colour. */
s32 wldcore_window_append_record_and_reset_color(u32** queue, s32* count) {
    wldcore_window_entry_36_rgb_t* color;

    queue[*count] = &g_wldcore_window_records[g_wldcore_window_record_count].flags;
    color = (wldcore_window_entry_36_rgb_t*)&g_wldcore_window_records->color;
    g_wldcore_window_records[g_wldcore_window_record_count].palette = 0;
    g_wldcore_window_records[g_wldcore_window_record_count].flags &= ~0x418;
    color[g_wldcore_window_record_count].red = 0x80;
    color[g_wldcore_window_record_count].green = 0x80;
    color[g_wldcore_window_record_count].blue = 0x80;
    *count += 1;
    return g_wldcore_window_record_count++;
}
