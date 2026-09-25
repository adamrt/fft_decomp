#include "fft/world.h"

/* Memory card slot selection step: waits 0x28 frames, opens menu thread 6,
 * then applies the chosen slot (g_world_menu_selection_results[5]) or reports cancel (9). */
void world_card_run_slot_select_step(void) {
    s16* selected;
    s32 slot;

    if (g_world_card_slot_select_running == 0) {
        if (g_world_card_slot_select_timer < 0x28) {
            g_world_card_slot_select_timer++;
            return;
        }
        g_world_card_slot_select_running = 1;
        g_world_card_save_mode = 0xff;
        g_world_card_slot_cursor_row = 0;
        g_world_menu_selection_results[5] = -1;
        g_world_card_slot_select_menu.selection = g_card_save_selected_slot;
        return;
    }
    g_world_card_slot_select_running = world_menu_run_thread(6, &g_world_card_slot_select_menu);
    if (g_world_card_slot_select_running == 0) {
        selected = &g_world_menu_selection_results[5];
        slot = *selected;
        if (slot == -1) {
            g_world_card_menu_step = 9;
        } else {
            g_world_text_substitution_values[0] = slot + 1;
            if (g_card_save_selected_slot != slot) {
                g_card_save_last_written_slot = 0xff;
            }
            g_card_save_selected_slot = *selected;
            /* The target passes slot without the definition's u8 narrowing. */
            ((void (*)(s32))world_card_set_selected_slot)(slot);
            g_world_card_menu_step = 2;
        }
    }
}
