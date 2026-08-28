#include "fft/wldcore.h"
#include "psx/types.h"

/* Initializes window record `index`: sets flags 0x810, stores its animation
 * sequence and OT priority, and restarts the animation. */
void wldcore_window_init_record(s32 index, s32 priority, s32 sequence) {
    g_wldcore_window_records[index].sequence = sequence;
    g_wldcore_window_records[index].priority = priority;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;
    g_wldcore_window_records[index].flags |= 0x810;
}
