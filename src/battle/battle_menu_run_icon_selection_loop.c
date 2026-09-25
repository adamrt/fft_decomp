#include "fft/battle.h"
#include "fft/battle_menu_window.h"
#include "fft/main_heap.h"
#include "fft/menu.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Menu icon list with a wrapping cursor, run inline by its caller's thread:
 * builds the text image once, alternates two sprite records and clears their
 * 0x78 state words when they hold 0 and 2. Exits the thread afterwards only
 * when the thread's second parameter is zero.
 *
 * BATTLE twin of world_menu_run_icon_selection_loop. */
void battle_menu_run_icon_selection_loop(void) {
    RECT rect;
    battle_menu_window_record_t records[2];
    s32 cursor;
    world_menu_icon_thread_param_t* param;
    void* buffer;
    s32 i;
    battle_menu_window_record_t* record;

    param = (world_menu_icon_thread_param_t*)battle_thread_get_current_parameter_1();
    record = &records[0];
    cursor = param->cursor;
    if (cursor == -1) {
        cursor = 0;
    }
    battle_menu_build_window_sprites((battle_menu_window_header_t*)&rect, (battle_menu_window_spec_t*)param, record);
    battle_copy_bytes(&records[1], record, 0x7C);
    buffer = battle_menu_build_and_upload_window_frame_image(param->width, param->height, &rect, 1);
    g_menu_text_state.stride = param->width;
    battle_menu_set_text_origin(8, 9);
    g_menu_text_palette_offset = 0;
    battle_menu_display_text_entry(param->text_id, buffer, &g_menu_text_state.origin_x);
    LoadImage(&rect, buffer);
    for (i = 0;; i++) {
        battle_thread_yield();
        if (i == 0) {
            battle_menu_free_memory(buffer);
        }
        record = &records[i & 1];
        param->cursor = cursor;
        battle_menu_configure_frame_cluts(record);
        battle_update_menu_cursor_primitives(param, (world_menu_icon_sprites_t*)record, i, cursor);
        if ((records[0].unknown_78 == 0 && records[1].unknown_78 == 2)
            || (records[0].unknown_78 == 2 && records[1].unknown_78 == 0)) {
            records[0].unknown_78 = 0;
            records[1].unknown_78 = 0;
        }
        if (battle_menu_should_close_thread(&g_battle_script_event_input) != 0) {
            break;
        }
        battle_menu_handle_action(param, cursor);
        battle_menu_update_selection_from_input((battle_menu_idle_action_entry_t*)param, &cursor);
        battle_handle_menu_cancel_input(param);
        battle_menu_submit_frame_primitives(record);
    }
    battle_thread_yield();
    if (battle_thread_get_current_parameter_2() == 0) {
        battle_thread_exit_current();
    }
}
