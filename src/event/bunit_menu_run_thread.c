#include "fft/event_bunit.h"
#include "psx/types.h"

struct world_menu_icon_thread_param;
struct world_menu_icon_sprites;

/* Menu window thread: rebuilds the text image whenever the entry's refresh
 * flag is raised and alternates between two sprite records until the menu
 * closes. The BUNIT twin of world_menu_icon_list_thread. */
void bunit_menu_run_thread(void) {
    RECT rect;
    battle_menu_window_record_t records[2];
    s32 cursor;
    battle_menu_idle_action_entry_t* state;
    u32* buffer;
    s32 i;
    s32 fresh;
    battle_menu_window_record_t* record;
    s16* redraw;

    g_bunit_input_controller = battle_script_get_controller_input_pointer(0);
    state = (battle_menu_idle_action_entry_t*)battle_thread_get_current_parameter_1();
    state->flags_0a &= ~1;
    cursor = state->selected_index;
    record = &records[0];
    if (cursor == -1) {
        cursor = 0;
    }
    battle_menu_build_window_sprites((battle_menu_window_header_t*)&rect, (battle_menu_window_spec_t*)state, record);
    battle_copy_bytes(&records[1], record, sizeof(records[1]));
    fresh = 0;
    redraw = state->refresh_flag;
    *redraw = 1;
    for (i = 0;; i++) {
        if (*redraw == 1) {
            *redraw = 0;
            buffer = battle_menu_build_and_upload_window_frame_image(state->inner_width, state->inner_height, &rect, 1);
            fresh = 1;
            g_menu_text_state.stride = state->inner_width;
            battle_menu_set_text_origin(8, 9);
            g_menu_text_palette_offset = 0;
            battle_menu_display_text_entry(state->text_id, buffer, &g_menu_text_state.origin_x);
            LoadImage(&rect, buffer);
        }
        record = &records[i & 1];
        battle_thread_yield();
        if (fresh == 1) {
            battle_menu_free_memory(buffer);
            fresh = 0;
        }
        state->selected_index = cursor;
        if (battle_menu_should_close_thread(g_bunit_input_controller) != 0) {
            break;
        }
        battle_menu_update_selection_from_input(state, &cursor);
        battle_menu_handle_action(state, cursor);
        battle_handle_menu_cancel_input(state);
        battle_menu_configure_frame_cluts(record);
        battle_update_menu_cursor_primitives(
            (struct world_menu_icon_thread_param*)state, (struct world_menu_icon_sprites*)record, i, cursor);
        battle_menu_submit_frame_primitives(record);
    }
    battle_thread_yield();
    battle_thread_exit_current();
}
