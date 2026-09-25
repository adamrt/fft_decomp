#include "fft/battle.h"
#include "fft/battle_menu_window.h"
#include "fft/menu.h"
#include "psx/types.h"

/* Menu window thread without a cursor: rebuilds the text image every seventh
 * frame and alternates between two sprite records until the menu closes. The
 * REQUIRE twin of world_menu_icon_strip_thread. */
void require_menu_run_simple_selection_thread(void) {
    RECT rect;
    battle_menu_window_record_t records[2];
    s32 input;
    battle_menu_idle_action_entry_t* state;
    u32* buffer;
    s32 i;
    s32 fresh;
    battle_menu_window_record_t* record;

    input = 0;
    state = (battle_menu_idle_action_entry_t*)battle_thread_get_current_parameter_1();
    record = &records[0];
    battle_menu_build_window_sprites((battle_menu_window_header_t*)&rect, (battle_menu_window_spec_t*)state, record);
    battle_copy_bytes(&records[1], record, sizeof(records[1]));
    fresh = 1;
    for (i = 0;; i++) {
        if (i % 7 == 0) {
            fresh = 1;
            buffer = battle_menu_build_and_upload_window_frame_image(state->inner_width, state->inner_height, &rect, 1);
            battle_menu_set_text_origin(8, 9);
            g_menu_text_state.stride = state->inner_width;
            battle_menu_display_text_entry(state->text_id, buffer, &g_menu_text_state.origin_x);
            LoadImage(&rect, buffer);
        }
        battle_thread_yield();
        if (fresh == 1) {
            battle_menu_free_memory(buffer);
            fresh = 0;
        }
        if (battle_menu_should_close_thread(&input) != 0) {
            break;
        }
        record = &records[i & 1];
        battle_menu_configure_frame_cluts(record);
        battle_menu_submit_frame_primitives(record);
    }
    battle_thread_yield();
    battle_thread_exit_current();
}
