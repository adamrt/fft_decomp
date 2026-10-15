#include "fft/battle.h"
#include "fft/battle_menu.h"
#include "fft/battle_unit.h"
#include "fft/menu.h"
#include "fft/thread.h"

enum {
    IDLE_ACTION_MENU_ENTRY_COUNT = 0x34,
};

/* Open an idle action menu (battle_menu_id_e) for the active-turn unit: place the
 * entry windows beside the unit, restore the row the unit last chose and start
 * the menu-building thread on menu_start_entry_thread.
 *
 * Nothing happens while menu input is blocked. The menu is also skipped while the
 * menus are suspended, a help window is open,
 * g_battle_menu_building_thread_started is set (it is cleared here either way) or
 * menu_id is BATTLE_MENU_ID_NONE.
 *
 * g_battle_menu_idle_action_entry_labels holds two window x bytes per entry, each
 * 0x80 less than the screen x; the second is used when the unit's projected x is
 * 0x101 or more, and a zero byte leaves the entry's x unchanged. */
void menu_build_idle_action_menu_variant(s32 menu_id) {
    SVECTOR screen_position;
    s32 i;
    s32 column;
    u8* label_table;
    menu_entry_t* entry;
    s16* selected_index;

    if (g_menu_input_mode != MENU_INPUT_BLOCKED) {
        if ((g_menu_overlay_state == 0) && (g_menu_help_open == 0) && (g_menu_building_thread_started == 0)
            && (menu_id != BATTLE_MENU_ID_NONE)) {
            g_menu_current_id = menu_id;
            g_menu_thread_menu_data = g_menu_idle_action_entries;
            unit_project_to_screen(
                battle_unit_get_runtime_id_by_battle_id(g_menu_selected_unit.battle_id), &screen_position);
            i = 0;
            entry = g_menu_thread_menu_data;
            label_table = g_battle_menu_idle_action_entry_labels;
            /* Two window x bytes per entry; a projected x of 0x101 or more uses the second. */
            column = screen_position.vx >= 0x101;
            do {
                /* The offset-plus-base integer sum is byte-required (addu operand order). */
                u8* label = (u8*)(column + (u32)label_table);

                column += 2;
                if (*label != 0) {
                    entry->window.x = *label + 0x80;
                }
                i += 1;
                entry += 1;
            } while (i < IDLE_ACTION_MENU_ENTRY_COUNT);
            menu_restore_selection_for_selected_unit(menu_id);
            selected_index = &g_menu_thread_menu_data[8].selected_index;
            *selected_index = 0;
            g_menu_status_list_layout.row_offset = 0;
            menu_clear_entry_selections();
            thread_start(THREAD_SLOT_MENU_BUILDING, menu_start_entry_thread);
            thread_set_parameters(THREAD_SLOT_MENU_BUILDING, menu_id, 0, 0);
        }
        g_menu_building_thread_started = 0;
    }
}
