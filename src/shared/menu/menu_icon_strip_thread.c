#include "fft/battle_menu.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/thread.h"

/* Menu window thread without a cursor: rebuilds the text image every seventh
 * frame and alternates between two sprite records until the menu closes. */
void menu_icon_strip_thread(void) {
    RECT rect;
    menu_icon_record_t records[2];
    u32 input;
    menu_entry_t* state;
    u8* buffer;
    s32 i;
    s32 fresh;
    menu_icon_record_t* record;

    input = 0;
    state = (menu_entry_t*)thread_get_current_parameter_1_variant();
    record = &records[0];
    menu_build_window_sprites(&rect, state, record);
    mem_copy_bytes_nonempty(&records[1], record, sizeof(records[1]));
    fresh = 1;
    for (i = 0;; i++) {
        if (i % 7 == 0) {
            fresh = 1;
            buffer = battle_menu_build_and_upload_window_frame_image(
                state->inner_width, state->inner_height, &rect, MENU_WINDOW_FRAME_BUILD);
            menu_set_text_origin(8, 9);
            g_menu_text_state.pen.stride = state->inner_width;
            menu_display_text_entry(state->text_id, buffer, &g_menu_text_state.pen);
            LoadImage(&rect, (u32*)buffer);
        }
        thread_yield();
        if (fresh == 1) {
            menu_free_memory(buffer);
            fresh = 0;
        }
        if (menu_should_close_thread(&input) != 0) {
            break;
        }
        record = &records[i & 1];
        menu_select_icon_cluts(&record->base);
        menu_submit_icon_primitives(&record->base);
    }
    thread_yield();
    thread_exit_current();
}
