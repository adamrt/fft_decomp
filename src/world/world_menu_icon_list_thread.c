#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Menu icon list thread with a wrapping cursor: rebuilds the text image
 * whenever the parameter's redraw flag is raised and alternates between two
 * sprite records until the input check completes. */
void world_menu_icon_list_thread(void) {
    RECT rect;
    world_menu_icon_record_t records[2];
    s32 cursor;
    world_menu_icon_thread_param_t* param;
    void* buffer;
    s32 i;
    s32 fresh;
    world_menu_icon_record_t* record;
    s16* redraw;

    g_world_input_frame_controller_input = world_input_get_menu_controller(0);
    param = world_thread_get_current_parameter_1();
    param->flags &= ~1;
    cursor = param->cursor;
    record = &records[0];
    if (cursor == -1) {
        cursor = 0;
    }
    world_menu_build_icon_record(&rect, param, record);
    world_script_copy_bytes(&records[1], record, 0x7C);
    fresh = 0;
    redraw = param->redraw_flag;
    *redraw = 1;
    for (i = 0;; i++) {
        if (*redraw == 1) {
            *redraw = 0;
            buffer = world_menu_build_and_upload_window_frame_image(param->width, param->height, &rect, 1);
            fresh = 1;
            g_world_menu_text_state.stride = param->width;
            world_menu_set_text_origin(8, 9);
            g_world_menu_text_color = 0;
            world_menu_display_text_entry(param->text_id, buffer, &g_world_menu_text_state.origin_x);
            LoadImage(&rect, buffer);
        }
        record = &records[i & 1];
        world_thread_yield();
        if (fresh == 1) {
            world_menu_free_memory(buffer);
            fresh = 0;
        }
        param->cursor = cursor;
        if (world_menu_check_thread_completion(g_world_input_frame_controller_input) != 0) {
            break;
        }
        world_menu_step_wrapping_cursor_on_scroll_buttons((world_menu_wrapping_cursor_bounds_t*)param, &cursor);
        world_menu_handle_entry_confirm((world_menu_entry_t*)param, cursor);
        world_menu_cancel_thread_group((world_menu_cancel_context_t*)param);
        world_menu_select_icon_cluts(&record->base);
        world_menu_update_icon_cursor_sprites(param, &record->base, i, cursor);
        world_menu_submit_icon_primitives(&record->base);
    }
    world_thread_yield();
    world_thread_exit_current();
}
