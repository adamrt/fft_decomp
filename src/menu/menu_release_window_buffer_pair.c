#include "fft/menu.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Release the menu window buffer pair whose first buffer is `buffer`: both slot
 * pointers become -1 and the active and thread_id 32-bit values of both buffers are
 * cleared. -1 is ignored; a buffer that is not the first of one of the three
 * pairs stops the current thread.
 *
 * BATTLE twin of menu_release_window_buffer_pair. */
void menu_release_window_buffer_pair(menu_window_buffer_t* buffer) {
    s32 slot;
    menu_window_buffer_t* empty;

    if (buffer == (menu_window_buffer_t*)-1) {
        return;
    }

    slot = 0;
    empty = (menu_window_buffer_t*)-1;
    for (; slot < 6; slot += 2) {
        if (g_menu_window_buffer_slots[slot] == buffer) {
            g_menu_window_buffer_slots[slot] = empty;
            g_menu_window_buffer_slots[slot + 1] = empty;
            g_menu_window_buffers[slot].active = 0;
            g_menu_window_buffers[slot + 1].active = 0;
            g_menu_window_buffers[slot].thread_id = 0;
            g_menu_window_buffers[slot + 1].thread_id = 0;
            return;
        }
    }

    thread_exit_current();
}
