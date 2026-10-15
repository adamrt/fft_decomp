/* Entry chain threads, text entry thread and digit resize. */

#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Walks a menu entry chain upward through the parent-index table, starting
 * a thread for each entry, until a root-type thread entry is reached. */
void world_menu_start_entry_chain_threads(u32 index, s32 thread) {
    menu_entry_t* entry;

    do {
        entry = &g_menu_thread_menu_data[index];
        thread_start(thread, entry->thread_entry);
        thread_set_parameters(thread, (s32)entry, 0, 0);
        if (entry->thread_entry == world_menu_labeled_number_entry_thread
            || entry->thread_entry == world_menu_number_entry_thread) {
            return;
        }
        index = entry->row_actions[entry->selected_index];
        thread--;
    } while (index < 1000);
}

/* Menu thread body: runs the entry's text binding hook, starts the text
 * thread for it, then idles until input arrives or the previous thread's
 * third parameter is raised. */
void world_menu_text_entry_thread(void) {
    thread_t* thread = &g_threads[g_thread_current_id];
    menu_entry_t* entry = (menu_entry_t*)thread->function_parameter_1;
    menu_text_binding_t* binding;
    s32 text_thread;
    u16 second;
    s32 speaker_id;
    u16 text_id;
    menu_text_binding_t* prepare_binding;

    prepare_binding = entry->context;
    prepare_binding->prepare();
    g_world_menu_list_controller_input = script_get_controller_input_pointer(0);
    text_id = entry->text_id;
    second = entry->header_id;
    binding = entry->context;
    text_thread = binding->text_thread_id;
    speaker_id = binding->speaker_id;
    thread_start(text_thread, text_run_dialog_thread_variant);
    thread_set_parameters(text_thread, second, text_id, speaker_id);
    thread_wait_until_inactive(text_thread);
    *g_world_menu_list_controller_input = PSX_PAD_CIRCLE;
    while (1) {
        menu_handle_action(entry, 0);
        if (menu_should_close_thread_variant(g_world_menu_list_controller_input) != 0) {
            break;
        }
        if ((thread - 1)->function_parameter_3 != 0) {
            break;
        }
        thread_wait_frames(1);
    }
    thread_exit_current();
}

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
