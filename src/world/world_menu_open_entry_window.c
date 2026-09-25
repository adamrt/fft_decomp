#include "fft/world.h"
#include "psx/types.h"

/* Window-origin view of world_menu_entry_t. The target clamps 0x08 as a signed
 * halfword, while the shared header types window_x as u16. */
typedef struct world_menu_window_entry {
    u8 unknown_00[8];
    s16 window_x; /* 0x08: world_menu_entry_t::window_x */
    s16 window_y; /* 0x0a: world_menu_entry_t::window_y */
    s16 width;    /* 0x0c: world_menu_entry_t::window_width */
    s16 height;   /* 0x0e: world_menu_entry_t::window_height */
    u8 unknown_10[0x3c - 0x10];
} world_menu_window_entry_t;
typedef char world_menu_window_entry_size_must_be_0x3c[(sizeof(world_menu_window_entry_t) == 0x3c) ? 1 : -1];

/* Opens menu entry `index` on thread 8, first placing its window at the default
 * origin and clamping that origin so the window stays on screen. */
void world_menu_open_entry_window(s32 index) {
    world_menu_window_entry_t* entry;
    s32 x;
    s32 y;
    s32 width;
    s32 height;
    s32 initial_x = -0x70;
    s32 initial_y = -0x50;

    if (g_world_threads[8].is_running == 0) {
        entry = (world_menu_window_entry_t*)&g_world_menu_default_entries[index];
        g_world_menu_thread_menu_data = (world_menu_entry_t*)g_world_menu_default_entries;
        entry->window_x = initial_x;
        entry->window_y = initial_y;
        x = entry->window_x;
        g_world_menu_open_entry_index = index;
        width = entry->width;
        if (x + width >= 0x81) {
            entry->window_x = 0x78 - entry->width;
        } else if (x < -0x80) {
            entry->window_x = -0x78;
        }
        height = entry->height;
        y = entry->window_y;
        if (y + height >= 0x79) {
            entry->window_y = 0x70 - entry->height;
        } else if (y < -0x78) {
            entry->window_y = -0x70;
        }
        world_thread_start(8, g_world_menu_thread_menu_data[index].thread_entry);
        world_thread_set_parameters(8, (s32)&g_world_menu_thread_menu_data[index], 0, 0);
    }
}
