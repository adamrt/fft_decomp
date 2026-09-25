#include "fft/battle.h"
#include "fft/battle_menu_window.h"
#include "fft/main_heap.h"
#include "fft/menu.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

struct battle_text_message_window_layout;
extern void battle_text_layout_message_window(
    struct battle_text_message_window_layout* window, s16* width, s16* height, s32* extra_frames, s32 pad_short);

/* Menu icon thread that also opens a linked entry: when parent_indices[0]
 * names a menu entry, that entry is centred on this text width, placed below
 * this entry, and started in the previous thread slot. The icon records then
 * alternate until input completes, selecting CLUTs in the linked slot.
 *
 * BATTLE twin of world_menu_icon_linked_entry_thread. */
void battle_menu_icon_linked_entry_thread(void) {
    RECT rect;
    battle_menu_window_record_t records[2];
    world_menu_entry_t* param;
    void* buffer;
    s32 i;
    s32 parent;
    battle_menu_window_record_t* record;
    s16 width;
    s16 height;
    s32 pad;

    param = (world_menu_entry_t*)battle_thread_get_current_parameter_1();
    battle_text_layout_message_window((struct battle_text_message_window_layout*)param, &width, &height, &pad, 0);
    battle_menu_build_window_sprites(
        (battle_menu_window_header_t*)&rect, (battle_menu_window_spec_t*)param, &records[0]);
    battle_copy_bytes(&records[1], &records[0], 0x7C);
    parent = param->parent_indices[0];
    if (parent >= 0) {
        g_battle_menu_thread_menu_data[parent].window_x
            = (width >> 1) - (g_battle_menu_thread_menu_data[parent].inner_width - 0x102);
        g_battle_menu_thread_menu_data[parent].window_y = param->window_y + param->window_height;
        battle_thread_start(g_battle_current_thread_id - 1, g_battle_menu_thread_menu_data[parent].thread_entry);
        battle_thread_set_parameters(
            g_battle_current_thread_id - 1, (s32)&g_battle_menu_thread_menu_data[parent], 0, 0);
    }
    buffer = battle_menu_build_and_upload_window_frame_image(width, height, &rect, 1);
    g_menu_text_state.stride = width;
    battle_menu_set_text_origin(8, 9);
    battle_menu_display_text_entry(param->text_id, buffer, &g_menu_text_state.origin_x);
    LoadImage(&rect, buffer);
    for (i = 0;; i++) {
        battle_thread_yield();
        if (i == 0) {
            battle_menu_free_memory(buffer);
        }
        if (battle_menu_should_close_thread(&g_battle_script_event_input) != 0) {
            break;
        }
        if (parent >= 0) {
            g_battle_current_thread_id--;
            battle_menu_configure_frame_cluts(&records[i & 1]);
            g_battle_current_thread_id++;
        } else {
            battle_menu_configure_frame_cluts(&records[i & 1]);
        }
        record = &records[i & 1];
        battle_update_menu_cursor_primitives(
            (world_menu_icon_thread_param_t*)param, (world_menu_icon_sprites_t*)record, i, -1);
        battle_menu_handle_action(param, 0);
        battle_handle_menu_cancel_input(param);
        battle_menu_submit_frame_primitives(record);
    }
    g_battle_sound_suppressed = 0;
    battle_thread_yield();
    if (battle_thread_get_current_parameter_2() == 0) {
        battle_thread_exit_current();
    }
}
