#include "fft/battle_menu.h"
#include "fft/battle_unit.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "psx/libgpu.h"

/* Run a wrapping icon menu; parameter 2 controls whether it exits its thread. */
void menu_run_icon_selection_loop(void) {
    RECT rect;
    menu_icon_record_t records[2];
    s32 cursor;
    menu_entry_t* param;
    u8* buffer;
    s32 i;
    menu_icon_record_t* record;

    param = (menu_entry_t*)thread_get_current_parameter_1_variant();
    record = &records[0];
    cursor = param->selected_index;
    if (cursor == -1) {
        cursor = 0;
    }
    menu_build_window_sprites_variant(&rect, param, record);
    mem_copy_bytes_nonempty(&records[1], record, sizeof(records[1]));
    buffer = menu_build_and_upload_window_frame_image_variant(
        param->inner_width, param->inner_height, &rect, MENU_WINDOW_FRAME_BUILD);
    g_menu_text_state.pen.stride = param->inner_width;
    menu_set_text_origin(8, 9);
    g_menu_text_palette_offset = 0;
    menu_display_text_entry(param->text_id, buffer, &g_menu_text_state.pen);
    LoadImage(&rect, (u32*)buffer);
    for (i = 0;; i++) {
        thread_yield();
        if (i == 0) {
            menu_free_memory(buffer);
        }
        record = &records[i & 1];
        param->selected_index = cursor;
        menu_select_icon_cluts(&record->base);
        menu_update_icon_cursor_sprites(param, &record->base, i, cursor);
        if ((records[0]._unknown_78 == 0 && records[1]._unknown_78 == 2)
            || (records[0]._unknown_78 == 2 && records[1]._unknown_78 == 0)) {
            records[0]._unknown_78 = 0;
            records[1]._unknown_78 = 0;
        }
        if (menu_should_close_thread_variant(&g_script_event_input) != 0) {
            break;
        }
        menu_handle_action(param, cursor);
        menu_step_wrapping_cursor_on_scroll_buttons(param, &cursor);
        menu_handle_cancel_input(param);
        menu_submit_icon_primitives(&record->base);
    }
    thread_yield();
    if (thread_get_current_parameter_2_variant() == 0) {
        thread_exit_current();
    }
}
