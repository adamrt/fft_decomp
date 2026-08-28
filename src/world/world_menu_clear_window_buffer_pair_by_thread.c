#include "fft/world.h"

/* Clear the pair whose first record's thread matches; the target forms one
 * 0x118-biased induction value across both records (compare 0x800e27f4). */
void world_menu_clear_window_buffer_pair_by_thread(s32 thread_id) {
    s32 index;

    for (index = 0; index < 6; index += 2) {
        if (g_world_menu_window_buffers[index].thread_id == thread_id) {
            g_world_menu_window_buffers[index].active = 0;
            g_world_menu_window_buffers[index + 1].active = 0;
            return;
        }
    }
}
