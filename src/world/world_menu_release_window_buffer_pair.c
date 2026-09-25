#include "fft/thread.h"
#include "fft/world.h"

/* Mark both entries unused and clear their headers; a missing key stops
 * the current thread (0x800e27f4). */
void world_menu_release_window_buffer_pair(menu_window_buffer_t* value) {
    s32 index;

    if (value != (menu_window_buffer_t*)-1) {
        /* One doubled index across both records lets GCC form the target's single
         * 0x118-biased induction value and four field displacements. */
        for (index = 0; index < 6; index += 2) {
            if (g_world_menu_window_buffer_pointers[index] == value) {
                g_world_menu_window_buffer_pointers[index] = (menu_window_buffer_t*)-1;
                g_world_menu_window_buffer_pointers[index + 1] = (menu_window_buffer_t*)-1;
                g_world_menu_window_buffers[index].active = 0;
                g_world_menu_window_buffers[index + 1].active = 0;
                g_world_menu_window_buffers[index].thread_id = 0;
                g_world_menu_window_buffers[index + 1].thread_id = 0;
                return;
            }
        }
        world_thread_exit_current();
    }
}
