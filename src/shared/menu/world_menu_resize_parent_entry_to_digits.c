#include "fft/menu.h"
#include "fft/thread.h"
#include "fft/world.h"

/* Resize the current thread's menu entry's first parent entry to fit a
 * decimal number, keeping its right edge fixed. */
void world_menu_resize_parent_entry_to_digits(void) {
    menu_entry_t* entry;
    u32 index;
    s32 digits;
    s32 value;
    s32 width;
    s32 old_width;
    s32 margin;

    entry = (menu_entry_t*)g_threads[g_thread_current_id].function_parameter_1;
    index = entry->row_actions[0];
    if (index < 1000) {
        entry = &g_menu_thread_menu_data[index];
        /* Preserve the original caller's unset argument. */
        digits = text_count_decimal_digits(value);
        width = digits * 6;
        margin = (digits & 1) * 2 + 0x18;
        width += margin;
        old_width = entry->inner_width;
        entry->inner_width = width;
        entry->window.w = width;
        entry->text_image.width = width;
        entry->window.x -= width - old_width;
    }
}
