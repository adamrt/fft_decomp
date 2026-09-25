#include "fft/thread.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/pad.h"
#include "psx/types.h"

typedef struct world_menu_confirm_entry world_menu_confirm_entry_t;
extern void world_menu_handle_entry_confirm(world_menu_confirm_entry_t* param, s32 value);

/* Menu icon list thread with a clamped cursor: builds the text image once,
 * steps the cursor on the up/down buttons while the previous thread is idle
 * and submits the four icon sprites of the alternating record each frame. */
void world_menu_icon_clamped_list_thread(void) {
    RECT rect;
    world_menu_icon_record_t records[2];
    world_menu_icon_thread_param_t* param;
    void* buffer;
    s32 i;
    s32 cursor;
    world_menu_icon_record_t* record;
    world_menu_icon_sprites_t* sprites;

    g_world_menu_list_controller_input = world_input_get_menu_controller(0);
    param = world_thread_get_current_parameter_1();
    cursor = param->cursor;
    record = &records[0];
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
        record = &records[i & 1];
        world_thread_yield();
        if (i == 0) {
            world_menu_free_memory(buffer);
        }
        param->cursor = cursor;
        if (world_menu_check_thread_completion(g_world_menu_list_controller_input) != 0) {
            break;
        }
        if (world_thread_is_previous_running() == 0) {
            if ((*g_world_menu_list_controller_input & PSX_PAD_UP) && cursor != 0) {
                cursor--;
            }
            if (!(*g_world_menu_list_controller_input & PSX_PAD_DOWN) && cursor != param->max_index) {
                cursor++;
            }
            world_menu_handle_entry_confirm((world_menu_confirm_entry_t*)param, cursor);
            world_menu_cancel_thread_group((world_menu_cancel_context_t*)param);
        }
        sprites = &record->base;
        world_menu_select_icon_cluts(sprites);
        world_menu_update_icon_cursor_sprites(param, &record->base, i, cursor);
        world_gfx_draw_or_append_gpu_primitive(&sprites->sprites[3].tag);
        world_gfx_draw_or_append_gpu_primitive(&sprites->sprites[2].tag);
        world_gfx_draw_or_append_gpu_primitive(&sprites->sprites[1].tag);
        world_gfx_draw_or_append_gpu_primitive(&sprites->draw_mode);
    }
    world_thread_yield();
    world_thread_exit_current();
}
