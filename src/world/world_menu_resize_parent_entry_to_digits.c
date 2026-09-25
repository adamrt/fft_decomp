#include "fft/world.h"

/* Resize the current thread's menu entry's first parent entry to fit a
 * decimal number, keeping its right edge fixed. */
void world_menu_resize_parent_entry_to_digits(void) {
    world_menu_entry_t* entry;
    s32 index;
    s32 digits;
    s32 width;
    s32 old_width;
    s32 margin;

    entry = (world_menu_entry_t*)g_world_threads[g_world_thread_current_id].function_parameter_1;
    index = entry->parent_indices[0];
    if ((u32)index < 1000) {
        entry = &g_world_menu_thread_menu_data[index];
        /* The target passes no argument; the callee reads whatever is in $a0. */
        digits = ((s32 (*)())world_text_count_decimal_digits)();
        width = digits * 6;
        margin = (digits & 1) * 2 + 0x18;
        width += margin;
        old_width = entry->inner_width;
        entry->inner_width = width;
        entry->window_width = width;
        entry->overall_width = width;
        entry->window_x -= width - old_width;
    }
}
