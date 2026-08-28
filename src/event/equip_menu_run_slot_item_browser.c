#include "fft/battle_text.h"
#include "fft/equip.h"
#include "fft/main_sound.h"
#include "fft/world.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Runs the per-slot item browser of the equipment screen.
 *
 * On entry it builds the selected slot's item list and installs the eight row
 * callbacks; each frame it handles cancel, left/right slot changes and
 * confirm, and rebuilds the stat-delta preview when the highlighted row
 * changes. `category` is u16 so its load is a zero_extend:HI that CSE cannot
 * replace with the value just stored to g_equip_selected_slot: the target
 * reloads the slot after the store. */
s32 equip_menu_run_slot_item_browser(void) {
    s16 selected;
    s16 scroll;
    u16 category;
    s32 input;
    s32 slot;

    if (g_equip_slot_item_browser_initialized == 0) {
        g_equip_slot_item_browser_initialized = 1;
        category = g_equip_selected_slot;
        g_equip_slot_item_browser_previewed_index = -1;
        /* The target passes sort_mode without the definition's u16 narrowing. */
        ((s32 (*)(s16, s32, s8, s16*, u8))equip_item_build_filtered_list)(g_equip_unit_selected_index, category + 1,
            g_equip_selected_slot != 4 ? category + 1 : 7, g_equip_item_list_entries, 0);
        equip_menu_restore_list_selection(g_equip_selected_slot, &selected, &scroll, g_equip_item_list_entries);
        equip_menu_init_scrollable_list(g_equip_item_list_entries, selected, scroll, g_battle_text_section_pointers[7]);
        g_equip_menu_list_row_callbacks[0] = equip_item_get_available_with_equip_flag;
        g_equip_menu_list_row_callbacks[1] = equip_item_get_total_with_equip_flag;
        g_equip_menu_list_row_callbacks[2] = (equip_row_callback_t)equip_item_build_row_icon_rect;
        g_equip_menu_list_row_callbacks[3] = (equip_row_callback_t)equip_item_build_row_graphic_descriptor;
        g_equip_menu_list_row_callbacks[4] = (equip_row_callback_t)equip_menu_get_preview_hp_bonus_display;
        g_equip_menu_list_row_callbacks[5] = (equip_row_callback_t)equip_menu_get_preview_mp_bonus_display;
        g_equip_menu_list_row_callbacks[6] = (equip_row_callback_t)equip_menu_get_preview_hp_bonus;
        g_equip_menu_list_row_callbacks[7] = (equip_row_callback_t)equip_menu_get_preview_mp_bonus;
    }
    equip_panel_toggle_item_numeric_thread(1);
    g_equip_text_help_message_id = g_equip_menu_list_entry_count != 0
        ? g_equip_item_list_entries[g_equip_menu_selected_list_index] + 0x6800
        : -1;
    input = g_equip_input_primary_repeat;
    if (input & PSX_PAD_CROSS) {
        equip_menu_set_selection_record(g_equip_selected_slot, g_equip_menu_selected_list_index,
            g_equip_menu_scroll_base_index, (u16*)g_equip_item_list_entries);
        g_equip_slot_item_browser_initialized = 0;
        return -1;
    }
    /* The target uses v0 from this void callee. */
    slot = ((s32 (*)(s32, s32, s32, s8))equip_menu_update_horizontal_selection_and_mark_change)(5, 2, input, 6);
    if (slot != g_equip_selected_slot) {
        equip_menu_set_selection_record(g_equip_selected_slot, g_equip_menu_selected_list_index,
            g_equip_menu_scroll_base_index, (u16*)g_equip_item_list_entries);
        g_equip_selected_slot = slot;
        category = g_equip_selected_slot;
        /* The target passes sort_mode without the definition's u16 narrowing. */
        ((s32 (*)(s16, s32, s8, s16*, u8))equip_item_build_filtered_list)(g_equip_unit_selected_index, category + 1,
            g_equip_selected_slot != 4 ? category + 1 : 7, g_equip_item_list_entries, 0);
        g_equip_slot_item_browser_previewed_index = -1;
        equip_menu_restore_list_selection(g_equip_selected_slot, &selected, &scroll, g_equip_item_list_entries);
        equip_menu_init_scrollable_list(g_equip_item_list_entries, selected, scroll, g_battle_text_section_pointers[7]);
        equip_gfx_set_transition_frame(10);
    } else if (g_equip_input_primary_repeat & PSX_PAD_CIRCLE) {
        if (g_equip_menu_list_entry_count != 0) {
            g_equip_sound_queued_effect_id = MAIN_SFX_CONFIRM;
            equip_menu_set_selection_record(slot, g_equip_menu_selected_list_index, g_equip_menu_scroll_base_index,
                (u16*)g_equip_item_list_entries);
            return 1;
        }
        g_equip_sound_queued_effect_id = MAIN_SFX_INVALID;
    }
    if (g_equip_menu_selected_list_index != g_equip_slot_item_browser_previewed_index) {
        if (g_equip_menu_list_entry_count != 0) {
            slot = g_equip_item_list_entries[g_equip_menu_selected_list_index];
        } else {
            slot = 0;
        }
        g_equip_slot_item_browser_previewed_index = g_equip_menu_selected_list_index;
        equip_collect_item_stat_deltas(
            (s16)slot, &g_equip_selected_unit_stat_summary, &g_equip_item_preview_stat_detail, 0);
        g_equip_unit_status_panel_redraw = 1;
        g_equip_item_numeric_panel_redraw = 1;
    }
    return 0;
}
