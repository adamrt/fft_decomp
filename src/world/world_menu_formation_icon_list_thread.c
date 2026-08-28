#include "fft/thread.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

typedef struct world_menu_confirm_entry world_menu_confirm_entry_t;
extern void world_menu_handle_entry_confirm(world_menu_confirm_entry_t* param, s32 value);
typedef struct world_menu_icon_prims_t world_menu_icon_prims_t;
extern void world_menu_submit_icon_primitives(world_menu_icon_prims_t* menu);

/* Formation menu icon list thread: draws the text image once, then
 * alternates two sprite records, tinting the four icon sprites with the
 * current menu colour and moving the wrapping cursor while the previous
 * thread is idle. */
void world_menu_formation_icon_list_thread(void) {
    RECT rect;
    u8 records[2][0x7C];
    s32 cursor;
    world_menu_icon_thread_param_t* param;
    void* buffer;
    s32 i;
    world_menu_icon_sprites_t* record;

    param = world_thread_get_current_parameter_1();
    record = (world_menu_icon_sprites_t*)records[0];
    cursor = param->cursor;
    if (cursor == -1) {
        cursor = 0;
    }
    world_menu_build_icon_record(&rect, param, record);
    SetShadeTex(&record->sprites[0], 0);
    SetShadeTex(&record->sprites[1], 0);
    SetShadeTex(&record->sprites[2], 0);
    SetShadeTex(&record->sprites[3], 0);
    world_script_copy_bytes(records[1], record, 0x7C);
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
        record = (world_menu_icon_sprites_t*)records[i & 1];
        param->cursor = cursor;
        world_menu_select_icon_cluts(record);
        world_menu_update_icon_cursor_sprites(param, (world_menu_icon_sprites_t*)record, i, cursor);
        if (world_menu_check_thread_completion(&g_world_formation_menu_input_state.new_buttons) != 0) {
            break;
        }
        if (world_thread_is_previous_running() == 0) {
            world_menu_step_wrapping_cursor_on_scroll_buttons((world_menu_wrapping_cursor_bounds_t*)param, &cursor);
            world_menu_handle_entry_confirm((world_menu_confirm_entry_t*)param, cursor);
            world_menu_cancel_thread_group((world_menu_cancel_context_t*)param);
        }
        record->sprites[0].r0 = g_world_menu_color_red;
        record->sprites[0].g0 = g_world_menu_color_green;
        record->sprites[0].b0 = g_world_menu_color_blue;
        record->sprites[1].r0 = g_world_menu_color_red;
        record->sprites[1].g0 = g_world_menu_color_green;
        record->sprites[1].b0 = g_world_menu_color_blue;
        record->sprites[2].r0 = g_world_menu_color_red;
        record->sprites[2].g0 = g_world_menu_color_green;
        record->sprites[2].b0 = g_world_menu_color_blue;
        record->sprites[3].r0 = g_world_menu_color_red;
        record->sprites[3].g0 = g_world_menu_color_green;
        record->sprites[3].b0 = g_world_menu_color_blue;
        world_menu_submit_icon_primitives((world_menu_icon_prims_t*)record);
    }
    world_thread_yield();
    world_thread_exit_current();
}
