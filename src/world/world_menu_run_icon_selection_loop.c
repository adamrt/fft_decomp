#include "fft/thread.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Menu icon list with a wrapping cursor, run inline by its caller's thread:
 * builds the text image once, alternates two sprite records and clears their
 * 0x78 state words when they hold 0 and 2. Exits the thread afterwards only
 * when the thread's second parameter is zero. */
void world_menu_run_icon_selection_loop(void) {
    RECT rect;
    world_menu_icon_record_t records[2];
    s32 cursor;
    world_menu_icon_thread_param_t* param;
    void* buffer;
    s32 i;
    world_menu_icon_record_t* record;

    param = world_thread_get_current_parameter_1();
    record = &records[0];
    cursor = param->cursor;
    if (cursor == -1) {
        cursor = 0;
    }
    world_menu_build_icon_record(&rect, param, record);
    world_script_copy_bytes(&records[1], record, 0x7C);
    buffer = world_menu_build_and_upload_window_frame_image(param->width, param->height, &rect, 1);
    g_world_menu_text_state.stride = param->width;
    world_menu_set_text_origin(8, 9);
    g_world_menu_text_color = 0;
    world_menu_display_text_entry(param->text_id, buffer, &g_world_menu_text_state.origin_x);
    LoadImage(&rect, buffer);
    for (i = 0;; i++) {
        world_thread_yield();
        if (i == 0) {
            world_menu_free_memory(buffer);
        }
        record = &records[i & 1];
        param->cursor = cursor;
        world_menu_select_icon_cluts(&record->base);
        world_menu_update_icon_cursor_sprites(param, &record->base, i, cursor);
        if ((records[0].unknown_78 == 0 && records[1].unknown_78 == 2)
            || (records[0].unknown_78 == 2 && records[1].unknown_78 == 0)) {
            records[0].unknown_78 = 0;
            records[1].unknown_78 = 0;
        }
        if (world_menu_check_thread_completion(&g_world_menu_new_button_input) != 0) {
            break;
        }
        world_menu_handle_entry_confirm((world_menu_entry_t*)param, cursor);
        world_menu_step_wrapping_cursor_on_scroll_buttons((world_menu_wrapping_cursor_bounds_t*)param, &cursor);
        world_menu_cancel_thread_group((world_menu_cancel_context_t*)param);
        world_menu_submit_icon_primitives(&record->base);
    }
    world_thread_yield();
    if (world_thread_get_current_parameter_2() == 0) {
        world_thread_exit_current();
    }
}
