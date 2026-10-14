#include "fft/gfx.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Menu icon list thread with a clamped cursor: builds the text image once,
 * steps the cursor on the up/down buttons while the previous thread is idle
 * and submits the four icon sprites of the alternating record each frame. */
void world_menu_icon_clamped_list_thread(void) {
    RECT rect;
    menu_icon_record_t records[2];
    menu_entry_t* param;
    u8* buffer;
    s32 i;
    s32 cursor;
    menu_icon_record_t* record;
    menu_icon_sprites_t* sprites;

    g_world_menu_list_controller_input = script_get_controller_input_pointer(0);
    param = (menu_entry_t*)world_thread_get_current_parameter_1();
    cursor = param->selected_index;
    record = &records[0];
    if (cursor == -1) {
        cursor = 0;
    }
    world_menu_build_icon_record(&rect, param, record);
    mem_copy_bytes_nonempty(&records[1], record, sizeof(records[1]));
    buffer = world_menu_build_and_upload_window_frame_image(
        param->inner_width, param->inner_height, &rect, MENU_WINDOW_FRAME_BUILD);
    g_menu_text_state.pen.stride = param->inner_width;
    world_menu_set_text_origin(8, 9);
    g_menu_text_palette_offset = 0;
    world_menu_display_text_entry(param->text_id, buffer, &g_menu_text_state.pen);
    LoadImage(&rect, buffer);
    for (i = 0;; i++) {
        record = &records[i & 1];
        thread_yield();
        if (i == 0) {
            menu_free_memory(buffer);
        }
        param->selected_index = cursor;
        if (menu_should_close_thread(g_world_menu_list_controller_input) != 0) {
            break;
        }
        if (world_thread_is_previous_running() == 0) {
            if ((*g_world_menu_list_controller_input & PSX_PAD_UP) && cursor != 0) {
                cursor--;
            }
            if (!(*g_world_menu_list_controller_input & PSX_PAD_DOWN) && cursor != param->max_row_index) {
                cursor++;
            }
            menu_handle_action(param, cursor);
            menu_handle_cancel_input(param);
        }
        sprites = &record->base;
        menu_select_icon_cluts(sprites);
        world_menu_update_icon_cursor_sprites(param, &record->base, i, cursor);
        gfx_draw_or_append_gpu_primitive(&sprites->sprites[3].tag);
        gfx_draw_or_append_gpu_primitive(&sprites->sprites[2].tag);
        gfx_draw_or_append_gpu_primitive(&sprites->sprites[1].tag);
        gfx_draw_or_append_gpu_primitive(&sprites->draw_mode);
    }
    thread_yield();
    thread_exit_current();
}
