/* No shared theme; window pair release plus the primitive gray-level setter. */

#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/types.h"

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

/* Set the color in a GPU primitive's common header to one gray level. */
void gfx_set_primitive_brightness(P_TAG* primitive, s32 brightness) {
    setRGB0(primitive, brightness, brightness, brightness);
}
