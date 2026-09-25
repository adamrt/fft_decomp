#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Menu icon thread that also opens a linked entry: when parent_indices[0]
 * names a menu entry, that entry is centred on this text width, placed below
 * this entry, and started in the previous thread slot. The icon records then
 * alternate until input completes, selecting CLUTs in the linked slot. */
void world_menu_icon_linked_entry_thread(void) {
    RECT rect;
    world_menu_icon_record_t records[2];
    world_menu_entry_t* param;
    void* buffer;
    s32 i;
    s32 parent;
    world_menu_icon_record_t* record;
    s16 width;
    s16 height;
    s32 pad;

    param = world_thread_get_current_parameter_1();
    world_menu_size_entry_to_text(param, &width, &height, &pad, 0);
    world_menu_build_icon_record(&rect, (world_menu_icon_thread_param_t*)param, &records[0]);
    world_script_copy_bytes(&records[1], &records[0], 0x7C);
    parent = param->parent_indices[0];
    if (parent >= 0) {
        g_world_menu_thread_menu_data[parent].window_x
            = (width >> 1) - (g_world_menu_thread_menu_data[parent].inner_width - 0x102);
        g_world_menu_thread_menu_data[parent].window_y = param->window_y + param->window_height;
        world_thread_start(g_world_thread_current_id - 1, g_world_menu_thread_menu_data[parent].thread_entry);
        world_thread_set_parameters(g_world_thread_current_id - 1, (s32)&g_world_menu_thread_menu_data[parent], 0, 0);
    }
    buffer = world_menu_build_and_upload_window_frame_image(width, height, &rect, 1);
    g_world_menu_text_state.stride = width;
    world_menu_set_text_origin(8, 9);
    world_menu_display_text_entry(param->text_id, buffer, &g_world_menu_text_state.origin_x);
    LoadImage(&rect, buffer);
    for (i = 0;; i++) {
        world_thread_yield();
        if (i == 0) {
            world_menu_free_memory(buffer);
        }
        if (world_menu_check_thread_completion(&g_world_menu_new_button_input) != 0) {
            break;
        }
        if (parent >= 0) {
            g_world_thread_current_id--;
            world_menu_select_icon_cluts(&records[i & 1].base);
            g_world_thread_current_id++;
        } else {
            world_menu_select_icon_cluts(&records[i & 1].base);
        }
        record = &records[i & 1];
        world_menu_update_icon_cursor_sprites((world_menu_icon_thread_param_t*)param, &record->base, i, -1);
        world_menu_handle_entry_confirm(param, 0);
        world_menu_cancel_thread_group((world_menu_cancel_context_t*)param);
        world_menu_submit_icon_primitives(&record->base);
    }
    g_world_menu_sound_muted = 0;
    world_thread_yield();
    if (world_thread_get_current_parameter_2() == 0) {
        world_thread_exit_current();
    }
}
