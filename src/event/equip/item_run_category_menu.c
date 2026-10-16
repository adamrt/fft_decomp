/* The equipment screen's item browser for one slot, with its stat change preview. */

#include "fft/event_equip.h"
#include "fft/item.h"
#include "fft/main_sound.h"
#include "fft/menu_help.h"
#include "fft/menu_input.h"
#include "fft/menu_panel.h"
#include "fft/menu_script.h"
#include "fft/menu_selection.h"
#include "fft/menu_status.h"
#include "fft/menu_window.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Runs the per-slot item browser of the equipment screen.
 *
 * On entry it builds the selected slot's item list and installs the eight row
 * callbacks; each frame it handles cancel, left/right slot changes and
 * confirm, and rebuilds the stat-delta preview when the highlighted row
 * changes. `category` is u16 so its load is a zero_extend:HI that CSE cannot
 * replace with the value just stored to g_menu_selected_slot: the target
 * reloads the slot after the store. */
s32 equipment_run_item_list(void) {
    s16 selected;
    s16 scroll;
    u16 category;
    s32 input;
    s32 slot;

    if (g_equipment_slot_item_browser_initialized == 0) {
        g_equipment_slot_item_browser_initialized = 1;
        category = g_menu_selected_slot;
        g_equipment_slot_item_browser_previewed_index = -1;
        equipment_build_item_list(g_equipment_unit_selected_index, category + 1,
            g_menu_selected_slot != ITEM_MENU_TAB_OTHER ? category + 1 : ITEM_LIST_FILTER_OTHER, g_item_list_entries,
            ITEM_LIST_MODE_ALL);
        menu_get_selection_record(g_menu_selected_slot, &selected, &scroll, g_item_list_entries);
        menu_init_scrollable_list(
            g_item_list_entries, selected, scroll, g_text_section_pointers[TEXT_SECTION_ITEM_NAMES]);
        g_menu_row_callbacks[0].value = equipment_get_row_equipped_count;
        g_menu_row_callbacks[1].value = equipment_get_row_item_count;
        g_menu_row_callbacks[2].icon = item_build_type_icon_source;
        g_menu_row_callbacks[3].icon = equipment_get_row_icon_source;
        g_menu_row_callbacks[4].value = equipment_get_preview_hp_bonus_display;
        g_menu_row_callbacks[5].value = equipment_get_preview_mp_bonus_display;
        g_menu_row_callbacks[6].value = equipment_get_preview_hp_bonus;
        g_menu_row_callbacks[7].value = equipment_get_preview_mp_bonus;
    }
    unit_panel_toggle_item_numbers_thread(1);
    /* The help text is the item's entry in HELP_TEXT_SECTION_ITEMS. */
    g_help_message_text_id
        = g_menu_list_entry_count != 0 ? g_item_list_entries[g_menu_list_selected_index] + HELP_TEXT_ID_ITEM_BASE : -1;
    input = g_menu_input_primary_repeat;
    if (input & PSX_PAD_CROSS) {
        menu_set_selection_record(
            g_menu_selected_slot, g_menu_list_selected_index, g_menu_scroll_base_index, g_item_list_entries);
        g_equipment_slot_item_browser_initialized = 0;
        return MENU_ITEM_LIST_STATE_CLOSE;
    }
    slot = menu_update_horizontal_selection_and_mark_change(5, 2, input, SOUND_SFX_PAGE_SWITCH);
    if (slot != g_menu_selected_slot) {
        menu_set_selection_record(
            g_menu_selected_slot, g_menu_list_selected_index, g_menu_scroll_base_index, g_item_list_entries);
        g_menu_selected_slot = slot;
        category = g_menu_selected_slot;
        equipment_build_item_list(g_equipment_unit_selected_index, category + 1,
            g_menu_selected_slot != ITEM_MENU_TAB_OTHER ? category + 1 : ITEM_LIST_FILTER_OTHER, g_item_list_entries,
            ITEM_LIST_MODE_ALL);
        g_equipment_slot_item_browser_previewed_index = -1;
        menu_get_selection_record(g_menu_selected_slot, &selected, &scroll, g_item_list_entries);
        menu_init_scrollable_list(
            g_item_list_entries, selected, scroll, g_text_section_pointers[TEXT_SECTION_ITEM_NAMES]);
        menu_set_window_scale_step(10);
    } else if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
        if (g_menu_list_entry_count != 0) {
            g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
            menu_set_selection_record(slot, g_menu_list_selected_index, g_menu_scroll_base_index, g_item_list_entries);
            return MENU_ITEM_LIST_STATE_REARRANGE;
        }
        g_sound_queued_effect_id = SOUND_SFX_INVALID;
    }
    /* A new highlighted row: preview its item (`slot` now holds the entry). */
    if (g_menu_list_selected_index != g_equipment_slot_item_browser_previewed_index) {
        if (g_menu_list_entry_count != 0) {
            slot = g_item_list_entries[g_menu_list_selected_index];
        } else {
            slot = 0;
        }
        g_equipment_slot_item_browser_previewed_index = g_menu_list_selected_index;
        item_collect_stat_deltas(slot, &g_unit_panel_selected_unit_stat_summary, &g_item_preview_stat_detail, 0);
        g_unit_panel_unit_status_panel_redraw = 1;
        g_unit_panel_preview_stats_thread_params.redraw_request = 1;
    }
    return MENU_ITEM_LIST_STATE_BROWSE;
}
