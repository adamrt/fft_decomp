#include "fft/wldcore.h"

/* Sibling of the resetter at 0x8007cd78, differing only in the value stored
 * into sequence. The record stores may alias `level`, so they invalidate
 * the cached parameter loads and reproduce the target's
 * reloads; `index` shares one address computation between the last two. */
void wldcore_window_publish_slot_b806(wldcore_menu_list_window_level_t* level) {
    s32 index;

    g_wldcore_window_records[level->main_window].palette = 10;
    g_wldcore_window_records[level->side_window].palette = 6;
    g_wldcore_window_render_records[level->frame_render].palette = 2;
    g_wldcore_window_records[level->main_window].sequence = 1;
    index = level->main_window;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;
}
