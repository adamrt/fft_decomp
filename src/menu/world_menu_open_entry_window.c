#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

/* Opens menu entry `index` on thread 8, first placing its window at the default
 * origin and clamping that origin so the window stays on screen. */
void world_menu_open_entry_window(s32 index, s32 unused_x, s32 unused_y) {
    menu_entry_t* entry;
    s32 x;
    s32 y;
    s32 initial_x = -0x70;
    s32 initial_y = -0x50;

    if (g_threads[THREAD_SLOT_MENU].is_running == 0) {
        entry = &g_world_menu_default_entries[index];
        g_menu_thread_menu_data = g_world_menu_default_entries;
        entry->window.x = initial_x;
        entry->window.y = initial_y;
        x = entry->window.x;
        g_world_menu_open_entry_index = index;
        if (x + entry->window.w >= 0x81) {
            entry->window.x = 0x78 - entry->window.w;
        } else if (x < -0x80) {
            entry->window.x = -0x78;
        }
        y = entry->window.y;
        if (y + entry->window.h >= 0x79) {
            entry->window.y = 0x70 - entry->window.h;
        } else if (y < -0x78) {
            entry->window.y = -0x70;
        }
        thread_start(THREAD_SLOT_MENU, g_menu_thread_menu_data[index].thread_entry);
        thread_set_parameters(THREAD_SLOT_MENU, (s32)&g_menu_thread_menu_data[index], 0, 0);
    }
}
