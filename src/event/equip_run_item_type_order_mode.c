#include "fft/battle_text.h"
#include "fft/equip.h"
#include "fft/main_sound.h"
#include "fft/world.h"
#include "psx/types.h"

/* Runs the item-type sort sub-menu for the selected equipment slot.
 *
 * A picked item type moves to the front of the slot's type-order list, after
 * which the item list is re-sorted and redrawn. Returns 5 while the sub-menu
 * thread runs and 2 once it closes. */
s32 equip_run_item_type_order_mode(void) {
    s16 names[8];
    s32 i;
    s32 picked;
    s32 first;
    u8 category;

    if (g_equip_type_order_ready == 0) {
        g_battle_text_section_pointers[25] = g_equip_item_type_order_names;
        g_equip_type_order_menu_hand_body.selected_index = 0;
        g_equip_type_order_menu_other_slots.selected_index = 0;
        equip_item_expand_type_order_list(g_equip_selected_slot + 1, g_equip_item_type_order);
        for (i = 0; (names[i] = g_equip_item_type_order[i]) != -1; i++) {
            names[i] += 3;
        }
        equip_text_concatenate_ids((s32)g_equip_text_data, g_equip_item_type_order_names, names, 1);
        g_equip_sound_queued_effect_id = MAIN_SFX_CONFIRM;
        g_equip_type_order_ready = 1;
    }
    if (g_equip_selected_slot == EQUIP_SLOT_RIGHT_HAND || g_equip_selected_slot == EQUIP_SLOT_BODY) {
        g_equip_item_type_order_active_menu = &g_equip_type_order_menu_hand_body;
    } else {
        g_equip_item_type_order_active_menu = &g_equip_type_order_menu_other_slots;
    }
    /* i doubles as the help-text id; the target reuses the same register. */
    i = g_equip_item_type_order[g_equip_item_type_order_active_menu->selected_index];
    if (i < 4) {
        i += 0x32;
    }
    if (i == 4) {
        i = 0x37;
    }
    if (i == 5) {
        i = 0x39;
    }
    picked = g_equip_menu_result_rows[2]; /* type-order row picked in the sub-menu, or -1 */
    g_equip_text_help_message_id = i + 0x1000;
    if (picked != -1) {
        first = g_equip_item_type_order[picked];
        for (i = picked - 1; i >= 0; i--) {
            g_equip_item_type_order[i + 1] = g_equip_item_type_order[i];
        }
        g_equip_item_type_order[0] = first;
        equip_menu_reset_selection_indices();
        equip_item_store_type_order_list(g_equip_selected_slot + 1, g_equip_item_type_order);
        for (i = 0; (names[i] = g_equip_item_type_order[i]) != -1; i++) {
            names[i] += 3;
        }
        equip_text_concatenate_ids((s32)g_equip_text_data, g_equip_item_type_order_names, names, 1);
        category = g_equip_selected_slot;
        g_equip_menu_text_redraw_flag = 1;
        /* The target passes sort_mode without the definition's u16 narrowing. */
        ((s32 (*)(s16, s32, s8, s16*, u8))equip_item_build_filtered_list)(g_equip_unit_selected_index, category + 0x81,
            g_equip_selected_slot != EQUIP_SLOT_ACCESSORY ? category + 1 : 7, g_equip_item_list_entries, 0);
        equip_item_store_category_list(g_equip_selected_slot, g_equip_item_list_entries);
        equip_menu_init_scrollable_list_core(
            g_equip_item_list_entries, (s16)g_equip_menu_selected_list_index, g_battle_text_section_pointers[7]);
        equip_gfx_set_transition_frame(10);
        equip_collect_item_stat_deltas(g_equip_item_list_entries[(s16)g_equip_menu_selected_list_index],
            &g_equip_selected_unit_stat_summary, &g_equip_item_preview_stat_detail, 0);
        g_equip_item_numeric_panel_redraw = 1;
    }
    if (equip_thread_start_managed(3, g_equip_item_type_order_active_menu) == 0) {
        g_equip_type_order_ready = 0;
        return 2;
    }
    return 5;
}
