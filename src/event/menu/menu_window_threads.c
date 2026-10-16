/* Icon list, text entry wait and icon strip threads. */

#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/script.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Thread body for a simple menu window: draw its text into the frame (again whenever the entry's
 * redraw flag is set), then move a wrapping cursor, act on confirm and cancel and animate the
 * cursor icon until the menu closes. */
void menu_icon_list_thread(void) {
    RECT rect;
    menu_icon_record_t records[2];
    s32 cursor;
    menu_entry_t* state;
    u8* buffer;
    s32 i;
    s32 fresh;
    menu_icon_record_t* record;
    s16* redraw;

    g_menu_input_controller = script_get_controller_input_pointer(0);
    state = (menu_entry_t*)thread_get_current_parameter_1();
    state->window.y &= ~1;
    cursor = state->selected_index;
    record = &records[0];
    if (cursor == -1) {
        cursor = 0;
    }
    menu_build_window_sprites(&rect, state, record);
    mem_copy_bytes_nonempty(&records[1], record, sizeof(records[1]));
    fresh = 0;
    redraw = state->context;
    *redraw = 1;
    for (i = 0;; i++) {
        if (*redraw == 1) {
            *redraw = 0;
            buffer = menu_build_and_upload_window_frame_image(
                state->inner_width, state->inner_height, &rect, MENU_WINDOW_FRAME_BUILD);
            fresh = 1;
            g_menu_text_state.pen.stride = state->inner_width;
            menu_set_text_origin(8, 9);
            g_menu_text_palette_offset = 0;
            menu_display_text_entry(state->text_id, buffer, &g_menu_text_state.pen);
            LoadImage(&rect, (u32*)buffer);
        }
        record = &records[i & 1];
        thread_yield();
        if (fresh == 1) {
            menu_free_memory(buffer);
            fresh = 0;
        }
        state->selected_index = cursor;
        if (menu_should_close_thread(g_menu_input_controller) != 0) {
            break;
        }
        menu_step_wrapping_cursor_on_scroll_buttons(state, &cursor);
        menu_handle_action(state, cursor);
        menu_handle_cancel_input(state);
        menu_select_icon_cluts(&record->base);
        menu_update_icon_cursor_sprites(state, &record->base, i, cursor);
        menu_submit_icon_primitives(&record->base);
    }
    thread_yield();
    thread_exit_current();
}

/*
 * Menu thread body for an entry that shows a message: runs the text
 * binding's prepare hook, forces a Circle press so the menu action fires at
 * once, starts the text thread named by the entry's text binding with the entry's
 * header and text ids and the binding's speaker, then waits for the previous
 * thread slot and the text thread to end.
 */
void menu_text_entry_wait_thread(void) {
    menu_entry_t* entry = (menu_entry_t*)g_threads[g_thread_current_id].function_parameter_1;
    menu_text_binding_t* text_binding;
    u32* input;
    u16 text_thread_id;
    u16 speaker_id;
    u16 header_id;
    u16 text_id;

    {
        menu_text_binding_t* binding = entry->context;

        binding->prepare();
    }
    input = script_get_controller_input_pointer(0);
    text_id = entry->text_id;
    text_binding = entry->context;
    header_id = entry->header_id;
    g_menu_input_controller = input;
    text_thread_id = text_binding->text_thread_id;
    speaker_id = text_binding->speaker_id;
    *input = PSX_PAD_CIRCLE;
    menu_handle_action(entry, 0);
    thread_start(text_thread_id, text_run_dialog_thread);
    thread_set_four_parameters(text_thread_id, header_id, text_id, speaker_id, speaker_id);
    thread_wait_until_inactive(g_thread_current_id - 1);
    thread_wait_until_inactive(text_thread_id);
    thread_exit_current();
}

/* Menu window thread without a cursor: rebuilds the text image every seventh
 * frame and alternates between two sprite records until the menu closes. */
void menu_icon_strip_thread(void) {
    RECT rect;
    menu_icon_record_t records[2];
    u32 input;
    menu_entry_t* state;
    u8* buffer;
    s32 i;
    s32 fresh;
    menu_icon_record_t* record;

    input = 0;
    state = (menu_entry_t*)thread_get_current_parameter_1();
    record = &records[0];
    menu_build_window_sprites(&rect, state, record);
    mem_copy_bytes_nonempty(&records[1], record, sizeof(records[1]));
    fresh = 1;
    for (i = 0;; i++) {
        if (i % 7 == 0) {
            fresh = 1;
            buffer = menu_build_and_upload_window_frame_image(
                state->inner_width, state->inner_height, &rect, MENU_WINDOW_FRAME_BUILD);
            menu_set_text_origin(8, 9);
            g_menu_text_state.pen.stride = state->inner_width;
            menu_display_text_entry(state->text_id, buffer, &g_menu_text_state.pen);
            LoadImage(&rect, (u32*)buffer);
        }
        thread_yield();
        if (fresh == 1) {
            menu_free_memory(buffer);
            fresh = 0;
        }
        if (menu_should_close_thread(&input) != 0) {
            break;
        }
        record = &records[i & 1];
        menu_select_icon_cluts(&record->base);
        menu_submit_icon_primitives(&record->base);
    }
    thread_yield();
    thread_exit_current();
}
