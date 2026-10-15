#include "fft/wldcore.h"
#include "psx/types.h"

/* Attach the current render record to the caller's list and reset it.
 *
 * Sibling of wldcore_window_append_record_and_reset_color, which performs the same append for the 36-byte
 * window records. */
s32 window_append_record_variant(void** queue, s32* count) {
    CVECTOR* color;

    queue[*count] = &g_wldcore_window_render_records[g_wldcore_window_render_record_count];
    /* Direct record.color access changes the column base and instruction scheduling. */
    color = &g_wldcore_window_render_records->color;
    g_wldcore_window_render_records[g_wldcore_window_render_record_count].flags
        &= ~(WORLD_RENDER_RECORD_BLANK | WORLD_RENDER_RECORD_HIDDEN | WORLD_RENDER_RECORD_UNTINTED);
    ((CVECTOR*)((u8*)color + g_wldcore_window_render_record_count * sizeof(g_wldcore_window_render_records[0])))->r
        = 0x80;
    ((CVECTOR*)((u8*)color + g_wldcore_window_render_record_count * sizeof(g_wldcore_window_render_records[0])))->g
        = 0x80;
    ((CVECTOR*)((u8*)color + g_wldcore_window_render_record_count * sizeof(g_wldcore_window_render_records[0])))->b
        = 0x80;
    g_wldcore_window_render_records[g_wldcore_window_render_record_count].palette = 0;
    g_wldcore_window_render_records[g_wldcore_window_render_record_count].anim_counter = 0;
    *count += 1;
    return g_wldcore_window_render_record_count++;
}
