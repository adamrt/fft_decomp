#include "fft/battle_menu.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"

/* Menu window thread: rebuilds the text image whenever the entry's refresh
 * flag is raised and alternates between two sprite records until the menu
 * closes. */
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
    state = (menu_entry_t*)battle_thread_get_current_parameter_1();
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
            buffer = battle_menu_build_and_upload_window_frame_image(
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
