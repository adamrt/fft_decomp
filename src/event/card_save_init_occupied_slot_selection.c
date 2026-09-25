#include "fft/event_card.h"
#include "psx/types.h"

s32 card_save_init_occupied_slot_selection(void) {
    s32 slot;
    s32 occupied_count;

    g_card_menu_script_callback = 0;
    slot = 0;
    occupied_count = 0;
    while (slot < 15) {
        if (slot == g_card_save_last_written_slot) {
            g_card_save_list_selected_slot = slot;
            g_card_save_list_cursor_index = occupied_count;
        }
        if (g_card_save_slot_file_states[slot] != -1) {
            occupied_count++;
        }
        slot++;
    }

    slot = 0;
    while (slot < 10) {
        card_save_update_slot_scroll(g_card_save_list_cursor_index);
        if (g_card_save_list_scroll_anim_offset == 0) {
            slot++;
        } else {
            slot = 0;
        }
    }
    return occupied_count;
}
