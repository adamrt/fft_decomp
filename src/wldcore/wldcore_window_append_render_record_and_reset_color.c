#include "fft/wldcore.h"
#include "psx/types.h"

/* Attach the current render record to the caller's list and reset it.
 *
 * Sibling of wldcore_window_append_record_and_reset_color, which performs the same append for the 36-byte
 * window records. */
s32 wldcore_window_append_render_record_and_reset_color(u32** queue, s32* count) {
    wldcore_window_entry_52_rgb_t* color;

    queue[*count] = (u32*)&g_wldcore_window_render_records[g_wldcore_window_render_record_count].flags;
    color = (wldcore_window_entry_52_rgb_t*)&g_wldcore_window_render_records->red;
    g_wldcore_window_render_records[g_wldcore_window_render_record_count].flags &= ~0x418;
    color[g_wldcore_window_render_record_count].red = 0x80;
    color[g_wldcore_window_render_record_count].green = 0x80;
    color[g_wldcore_window_render_record_count].blue = 0x80;
    g_wldcore_window_render_records[g_wldcore_window_render_record_count].palette = 0;
    g_wldcore_window_render_records[g_wldcore_window_render_record_count].anim_counter = 0;
    *count += 1;
    return g_wldcore_window_render_record_count++;
}
