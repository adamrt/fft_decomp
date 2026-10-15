#include "fft/world.h"
#include "psx/types.h"

/* Count the memory-card save slots that qualify for the current menu mode
 * (occupied slots when g_world_card_save_mode is nonzero, every available slot otherwise),
 * select the last written slot, then run the slot list update until it settles for ten
 * consecutive passes. Returns the number of qualifying slots. */
s32 world_card_count_selectable_save_slots(void) {
    s32 count;
    s32 i;

    g_world_menu_script_callbacks[0].value = 0;
    for (i = 0, count = 0; i < 15; i++) {
        if (i == g_main_card_save_last_used_slot) {
            g_world_card_active_slot = i;
            g_world_card_slot_cursor_row = count;
        }
        if (g_world_card_save_mode != 0) {
            if (g_main_card_save_slot_file_states[i] == CARD_SAVE_SLOT_OCCUPIED) {
                count++;
            }
        } else if (g_main_card_save_slot_file_states[i] != CARD_SAVE_SLOT_UNAVAILABLE) {
            count++;
        }
    }
    i = 0;
    while (i < 10) {
        world_card_scroll_menu_list(g_world_card_slot_cursor_row);
        if (g_world_card_list_scroll_velocity != 0) {
            i = 0;
        } else {
            i++;
        }
    }
    return count;
}
