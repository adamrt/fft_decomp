#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Formation menu icon list thread: draws the text image once, then
 * alternates two sprite records, tinting the four icon sprites with the
 * current menu color and moving the wrapping cursor while the previous
 * thread is idle. */
void world_menu_formation_icon_list_thread(void) {
    RECT rect;
    menu_icon_record_t records[2];
    s32 cursor;
    menu_entry_t* param;
    u8* buffer;
    s32 i;
    menu_icon_sprites_t* record;

    param = (menu_entry_t*)world_thread_get_current_parameter_1();
    record = &records[0].base;
    cursor = param->selected_index;
    if (cursor == -1) {
        cursor = 0;
    }
    world_menu_build_icon_record(&rect, param, &records[0]);
    SetShadeTex(&record->sprites[0], 0);
    SetShadeTex(&record->sprites[1], 0);
    SetShadeTex(&record->sprites[2], 0);
    SetShadeTex(&record->sprites[3], 0);
    mem_copy_bytes_nonempty(&records[1], record, sizeof(records[1]));
    buffer = world_menu_build_and_upload_window_frame_image(
        param->inner_width, param->inner_height, &rect, MENU_WINDOW_FRAME_BUILD);
    g_menu_text_state.pen.stride = param->inner_width;
    world_menu_set_text_origin(8, 9);
    g_menu_text_palette_offset = 0;
    world_menu_display_text_entry(param->text_id, buffer, &g_menu_text_state.pen);
    LoadImage(&rect, buffer);
    for (i = 0;; i++) {
        thread_yield();
        if (i == 0) {
            menu_free_memory(buffer);
        }
        record = &records[i & 1].base;
        param->selected_index = cursor;
        menu_select_icon_cluts(record);
        world_menu_update_icon_cursor_sprites(param, record, i, cursor);
        if (menu_should_close_thread(&g_world_formation_menu_input_state.new_buttons) != 0) {
            break;
        }
        if (world_thread_is_previous_running() == 0) {
            world_menu_step_wrapping_cursor_on_scroll_buttons(param, &cursor);
            menu_handle_action(param, cursor);
            menu_handle_cancel_input(param);
        }
        setRGB0(&record->sprites[0], g_world_menu_color_red, g_world_menu_color_green, g_world_menu_color_blue);
        setRGB0(&record->sprites[1], g_world_menu_color_red, g_world_menu_color_green, g_world_menu_color_blue);
        setRGB0(&record->sprites[2], g_world_menu_color_red, g_world_menu_color_green, g_world_menu_color_blue);
        setRGB0(&record->sprites[3], g_world_menu_color_red, g_world_menu_color_green, g_world_menu_color_blue);
        world_menu_submit_icon_primitives(record);
    }
    thread_yield();
    thread_exit_current();
}
