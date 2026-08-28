#include "fft/battle.h"

enum {
    IDLE_ACTION_MENU_ENTRY_COUNT = 0x34,
    IDLE_ACTION_MENU_INVALID_ID = -1,
    MENU_BUILDING_THREAD_ID = 3,
};

void battle_menu_build_idle_action_menu(s32 menu_id) {
    s16 screen_y;
    s32 i;
    s32 column;
    u8* label_table;
    world_menu_entry_t* entry;

    if (g_battle_menu_input_disabled != 1) {
        if ((g_option_menu_submenu_state == 0) && (g_battle_menu_help_open == 0)
            && (g_battle_menu_building_thread_started == 0) && (menu_id != IDLE_ACTION_MENU_INVALID_ID)) {
            g_battle_menu_current_id = menu_id;
            g_battle_menu_thread_menu_data = g_battle_menu_idle_action_entries;
            battle_unit_project_misc_to_screen(
                battle_unit_get_misc_id_by_battle_id(g_battle_active_turn_unit.battle_id), &screen_y);
            i = 0;
            entry = g_battle_menu_thread_menu_data;
            label_table = g_battle_menu_idle_action_entry_labels;
            /* Two label columns per entry; the lower half of the screen uses the second. */
            column = screen_y >= 0x101;
            do {
                u8* label = (u8*)((u32)column + (u32)label_table);

                column += 2;
                if (*label != 0) {
                    entry->window_x = *label + 0x80;
                }
                i += 1;
                entry += 1;
            } while (i < IDLE_ACTION_MENU_ENTRY_COUNT);
            /* The target passes three extra arguments the one-parameter callee ignores. */
            ((void (*)(s32, world_menu_entry_t*, s32, u8*))battle_menu_restore_selection_for_selected_unit)(
                menu_id, entry, i, label_table);
            /* Entry 8's selected_index. A typed member store moves into the following jal
             * delay slot; this raw access preserves the target's store ordering. */
            *(s16*)((u8*)g_battle_menu_thread_menu_data + 0x218) = 0;
            D_801669E8 = 0;
            battle_menu_clear_idle_action_menu_entry_flags();
            battle_thread_start(MENU_BUILDING_THREAD_ID, battle_menu_start_entry_thread);
            battle_thread_set_parameters(MENU_BUILDING_THREAD_ID, menu_id, 0, 0);
        }
        g_battle_menu_building_thread_started = 0;
    }
}
