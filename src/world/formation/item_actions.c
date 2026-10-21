/* The item action and sort order menus, units equipping the selected item, and the row window and cursor. */

#include "fft/formation.h"
#include "fft/formation_item.h"
#include "fft/gfx.h"
#include "fft/help.h"
#include "fft/item.h"
#include "fft/item_menu.h"
#include "fft/menu.h"
#include "fft/menu_gfx.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/menu_script.h"
#include "fft/menu_selection.h"
#include "fft/menu_window.h"
#include "fft/screen.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/unit_panel.h"
#include "psx/pad.h"
#include "psx/types.h"

/* item-action menus and category graphics */
extern screen_number_range_t g_formation_item_action_quantity_range;
extern s16 g_formation_item_action_option_states[3];
extern menu_entry_t g_formation_item_quantity_change_menu;
extern menu_entry_t g_formation_item_unequip_all_menu;
extern oriented_quad_t g_formation_item_category_tab_sprites[]; /* 20-byte quads, one per row */
extern u8 g_formation_item_category_tab_x[];                    /* per-row base x */
extern oriented_quad_t g_formation_item_category_tab_cursor_sprite;

/* The two item-category icon strips; the active one is kept in g_formation_item_category_icon_strip. */
extern menu_entry_t g_formation_item_category_icon_strip_a;
extern menu_entry_t g_formation_item_category_icon_strip_b;
static menu_entry_t* g_formation_item_category_icon_strip;
static u16 g_formation_item_saved_selected_unit_index;
extern s16 g_formation_item_category_strip_text_ids[]; /* option text ids of the active strip, -1 terminated */

/* Byte-aligned display streams; each extent includes the terminating opcode. */
extern u8 g_formation_item_category_tabs_script[121];
extern u8 g_formation_item_category_tabs_wide_script[121];

/*
 * Run the Items screen's action strip (MENU_ITEM_LIST_STATE_ACTION) and
 * return the next state: ACTION while it stays open, BROWSE when it is
 * canceled, SORT_ORDER once the sort option is chosen.
 *
 * On entry the three option lines are enabled or grayed from the selected
 * item: option 0 (discard a quantity) needs some in the inventory, option 1
 * (unequip it from every formation unit) needs it equipped somewhere, and
 * option 2 (sort order) needs at least two list entries. With no option
 * chosen, Left/Right switch the category tab.
 *
 * `item` first holds the chosen option and `selected` copies it: the target
 * compares g_formation_item_action_menu_initialized against that copy after `item` is reused for the item
 * id.
 */
s8 formation_items_action_update(void) {
    s16 selected_index;
    s16 scroll_base_index;
    u8* text;
    s32 result;
    s32 selected;
    s32 item;
    s32 i;
    s32 slot;

    if (g_formation_item_action_menu_initialized == 0) {
        g_formation_item_category_input_lock = 0;
        g_help_message_text_id = 0;
        screen_selection_results_reset();
        g_unit_panel_preview_stats_thread_params.style = 0;

        /* Each option line starts with a palette code (0xE3) whose operand 4 grays the line out;
         * option states of 0x1000 and up refuse Circle. */
        /* The text seek returns a read-only view; this loaded menu table is mutable. */
        text = (u8*)menu_text_find_entry_by_index(g_formation_menu_text_table, 3, 2);
        if (g_menu_list_entry_count != 0
            && (result = item_inventory_count_adjust(g_item_list_entries[g_menu_list_selected_index], 0)) != 0) {
            text[1] = 0;
            g_formation_item_action_option_states[0] = 1;
            g_formation_item_action_quantity_range.bounds.max = result;
        } else {
            text[1] = 4;
            g_formation_item_action_option_states[0] = 0x14D2;
        }

        text++;
        while (*text != TEXT_SET_PALETTE) {
            text++;
        }
        if (g_menu_list_entry_count != 0
            && item_equipped_count_get(g_item_list_entries[g_menu_list_selected_index]) != 0) {
            text[1] = 0;
            g_formation_item_action_option_states[1] = 4;
        } else {
            text[1] = 4;
            g_formation_item_action_option_states[1] = 0x14D2;
        }

        text++;
        while (*text != TEXT_SET_PALETTE) {
            text++;
        }
        if (g_item_list_entries[0] != -1 && g_item_list_entries[1] != -1) {
            text[1] = 0;
            g_formation_item_action_option_states[2] = -1;
        } else {
            text[1] = 4;
            g_formation_item_action_option_states[2] = 0x14D2;
        }

        g_screen_text_redraw_request = 1;
        g_formation_item_action_menu_initialized = 1;
    }

    result = menu_managed_thread_start(THREAD_SCREEN_SLOT_MENU, &g_item_action_menu);
    if (result == 0) {
        g_formation_item_action_menu_initialized = 0;
        if (g_screen_selection_results[3] == -1) {
            return MENU_ITEM_LIST_STATE_BROWSE;
        }
    }
    if (g_formation_item_action_option_states[g_item_action_menu.selected_index] >= 0x1000
        && (g_menu_input_primary_repeat & PSX_PAD_CIRCLE)) {
        g_sound_queued_effect_id = SOUND_SFX_INVALID;
    }

    item = g_screen_selection_results[3];
    selected = item;
    if (selected != -1) {
        if (selected == 0) {
            g_unit_panel_preview_stats_thread_params.style = 1;
            if (thread_is_running(THREAD_SCREEN_SLOT_MENU_CHILD) != 0) {
                return MENU_ITEM_LIST_STATE_ACTION;
            }
            if (g_screen_selection_results[5] == 0) {
                item_inventory_count_adjust(
                    g_item_list_entries[g_menu_list_selected_index], -g_screen_selection_results[4]);
                /* With none left anywhere, drop the entry from the list and rebuild around it. */
                if (item_total_count_get(g_item_list_entries[g_menu_list_selected_index]) == 0) {
                    for (i = g_menu_list_selected_index; i < g_menu_list_entry_count; i++) {
                        g_item_list_entries[i] = g_item_list_entries[i + 1];
                    }
                    screen_list_init(g_item_list_entries, g_menu_list_selected_index,
                        g_text_section_pointers[TEXT_SECTION_ITEM_NAMES],
                        (menu_instruction_t*)g_formation_item_list_window_script);
                    menu_set_window_scale_step(10);
                    item_rebuild_category_list(g_menu_selected_slot);
                    item_stat_preview_build(g_item_list_entries[g_menu_list_selected_index],
                        &g_unit_panel_selected_unit_stat_summary, &g_item_preview_stat_detail, 0);
                    g_unit_panel_preview_stats_thread_params.redraw_request = 1;
                }
            }
            g_formation_item_action_menu_initialized = 0;
            screen_selection_results_reset();
            g_unit_panel_preview_stats_thread_params.style = 0;
            g_formation_item_quantity_change_menu.selected_index = 0;
            return MENU_ITEM_LIST_STATE_ACTION;
        } else if (selected == 1) {
            g_unit_panel_preview_stats_thread_params.style = selected;
            item = g_item_list_entries[g_menu_list_selected_index];
            if (g_formation_item_action_menu_initialized == selected) {
                g_sound_suppressed = 1;
                if (g_screen_selection_results[5] == 0) {
                    g_sound_queued_effect_id = SOUND_SFX_UNEQUIP;
                } else if (g_screen_selection_results[5] == 1) {
                    g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
                }
                /* Once a sound is queued, 100 keeps the menu initialized but stops this block running again. */
                g_formation_item_action_menu_initialized = g_sound_queued_effect_id != SOUND_SFX_NONE ? 100 : 1;
            }
            if (thread_is_running(THREAD_SCREEN_SLOT_MENU_CHILD) != 0) {
                return MENU_ITEM_LIST_STATE_ACTION;
            }
            if (g_screen_selection_results[5] == 0) {
                for (i = 0; i < g_formation_unit_count; i++) {
                    for (slot = 0; slot < MENU_EQUIPMENT_SLOT_COUNT; slot++) {
                        if (item == g_formation_unit_pointers[i]->detail.equipment[slot]) {
                            formation_item_equip(i, slot, ITEM_ID_NOTHING);
                        }
                    }
                }
            }
            g_sound_suppressed = 0;
            screen_selection_results_reset();
            g_unit_panel_preview_stats_thread_params.style = 0;
            g_formation_item_unequip_all_menu.selected_index = 0;
        } else {
            if (result == 0) {
                return MENU_ITEM_LIST_STATE_SORT_ORDER;
            }
            return MENU_ITEM_LIST_STATE_ACTION;
        }
    } else {
        /* No option chosen: Left/Right switch the category tab, saving the old list's cursor. */
        if (thread_is_running(THREAD_SCREEN_SLOT_MENU_CHILD) != 0) {
            return MENU_ITEM_LIST_STATE_ACTION;
        }
        result = menu_selection_horizontal_sound_step(
            5, SCREEN_CURSOR_ITEM_TAB, g_menu_input_primary_repeat, SOUND_SFX_PAGE_SWITCH);
        if (result == g_menu_selected_slot) {
            return MENU_ITEM_LIST_STATE_ACTION;
        }

        g_formation_item_category_input_lock = 1;
        menu_selection_record_set(
            g_menu_selected_slot, g_menu_list_selected_index, g_menu_scroll_base_index, g_item_list_entries);
        g_menu_selected_slot = result;
        formation_item_list_build(g_unit_selected_index, g_menu_selected_slot + 1,
            g_menu_selected_slot != ITEM_MENU_TAB_OTHER ? g_menu_selected_slot + 1 : ITEM_LIST_FILTER_OTHER,
            g_item_list_entries, ITEM_LIST_MODE_ALL);
        menu_selection_record_restore(g_menu_selected_slot, &selected_index, &scroll_base_index, g_item_list_entries);
        menu_init_scrollable_list(
            g_item_list_entries, selected_index, scroll_base_index, g_text_section_pointers[TEXT_SECTION_ITEM_NAMES]);
        menu_set_window_scale_step(10);
        if (g_menu_list_entry_count != 0) {
            result = g_item_list_entries[g_menu_list_selected_index];
        } else {
            result = 0;
        }
        item_stat_preview_build(result, &g_unit_panel_selected_unit_stat_summary, &g_item_preview_stat_detail, 0);
        g_unit_panel_status_display_thread_params.redraw_request = 1;
        g_unit_panel_preview_stats_thread_params.redraw_request = 1;
    }

    g_formation_item_action_menu_initialized = 0;
    return MENU_ITEM_LIST_STATE_ACTION;
}

/*
 * Run the sort order strip for the selected tab and return the next state:
 * SORT_ORDER while the strip thread runs, ACTION once it closes.
 *
 * On first entry the tab's sort key list is loaded and its text built. A
 * chosen key moves to the front of the list, the list is stored back and the
 * tab's item list is re-sorted and stored. `i` also carries the description
 * text id; a separate variable changes the target's register allocation.
 */
s8 formation_items_sort_update(void) {
    s16 text_ids[8];
    s32 i;
    s32 index;
    s32 selected;

    if (g_formation_item_sort_order_initialized == 0) {
        g_formation_item_category_icon_strip_a.selected_index = 0;
        g_formation_item_category_icon_strip_b.selected_index = 0;
        item_load_type_order(g_menu_selected_slot + 1, g_formation_item_category_strip_text_ids);
        i = 0;
        while ((text_ids[i] = g_formation_item_category_strip_text_ids[i]) != -1) {
            text_ids[i] += 4;
            i++;
        }
        text_concatenate_entries(g_formation_menu_text_table, g_formation_menu_text_buffer, text_ids, 1);
        g_formation_item_sort_order_initialized = 1;
    }
    if (g_menu_selected_slot == 0 || g_menu_selected_slot == 3) {
        g_formation_item_category_icon_strip = &g_formation_item_category_icon_strip_a;
    } else {
        g_formation_item_category_icon_strip = &g_formation_item_category_icon_strip_b;
    }
    i = g_formation_item_category_strip_text_ids[g_formation_item_category_icon_strip->selected_index];
    if (i < 4) {
        i += 0x32;
    }
    if (i == 4) {
        i = 0x37;
    }
    if (i == 5) {
        i = 0x39;
    }
    g_help_message_text_id = i + HELP_TEXT_ID_MENU_COMMAND_BASE;
    index = g_screen_selection_results[7];
    if (index != -1) {
        selected = g_formation_item_category_strip_text_ids[index];
        for (i = index - 1; i >= 0; i--) {
            g_formation_item_category_strip_text_ids[i + 1] = g_formation_item_category_strip_text_ids[i];
        }
        g_formation_item_category_strip_text_ids[0] = selected;
        screen_selection_results_reset();
        item_store_type_order(g_menu_selected_slot + 1, g_formation_item_category_strip_text_ids);
        i = 0;
        while ((text_ids[i] = g_formation_item_category_strip_text_ids[i]) != -1) {
            text_ids[i] += 4;
            i++;
        }
        text_concatenate_entries(g_formation_menu_text_table, g_formation_menu_text_buffer, text_ids, 1);
        g_screen_text_redraw_request = 1;
        /* Sort mode bit 0x80 makes the builder sort afresh by the new key list instead of
         * keeping the tab's stored order. */
        formation_item_list_build(g_unit_selected_index, g_menu_selected_slot + 0x81,
            g_menu_selected_slot != ITEM_MENU_TAB_OTHER ? g_menu_selected_slot + 1 : ITEM_LIST_FILTER_OTHER,
            g_item_list_entries, ITEM_LIST_MODE_ALL);
        item_store_category_list(g_menu_selected_slot, g_item_list_entries);
        menu_scrollable_list_core_init(
            g_item_list_entries, g_menu_list_selected_index, g_text_section_pointers[TEXT_SECTION_ITEM_NAMES]);
        menu_set_window_scale_step(10);
        item_stat_preview_build(g_item_list_entries[g_menu_list_selected_index],
            &g_unit_panel_selected_unit_stat_summary, &g_item_preview_stat_detail, 0);
        g_unit_panel_preview_stats_thread_params.redraw_request = 1;
    }
    if (menu_managed_thread_start(THREAD_SCREEN_SLOT_MENU, g_formation_item_category_icon_strip) == 0) {
        g_formation_item_sort_order_initialized = 0;
        return MENU_ITEM_LIST_STATE_ACTION;
    }
    return MENU_ITEM_LIST_STATE_SORT_ORDER;
}

/*
 * Item menu "equipped by" view: browse the formation units that have the
 * highlighted item equipped.
 *
 * The first call rebuilds the formation list filtered by that item and turns
 * on unit browsing and the banner. Cross or Circle (once the comparison
 * banner thread is idle) restores the full list and the selected unit, stops
 * the three panel threads and returns MENU_ITEM_LIST_STATE_BROWSE; until then
 * the result is MENU_ITEM_LIST_STATE_EQUIPPED_BY.
 */
s32 formation_items_equipped_units_update(void) {
    if (g_formation_item_equipping_units_initialized == 0) {
        formation_save_records();
        g_formation_unit_count = formation_record_list_build(
            g_item_list_entries[g_menu_list_selected_index], g_formation_unit_pointers, FORMATION_RECORD_LIST_ALL);
        g_help_message_text_id = MENU_GRID_HELP_UNIT_BROWSE;
        g_formation_unit_browse_enabled = 1;
        g_formation_unit_banner_enabled = 1;
        formation_toggle_preview_stats_window(0);
        g_formation_item_saved_selected_unit_index = g_unit_selected_index;
        g_unit_selected_index = 0;
        formation_stage_selected_unit();
        g_formation_item_equipping_units_initialized = 1;
        menu_set_vsync_mode_and_event_speed(0);
        g_formation_unit_cycle_mode = 2;
    }
    if (thread_is_running(THREAD_SCREEN_SLOT_COMPARISON_STATUS) == 0) {
        if ((g_menu_input_newly_pressed & PSX_PAD_CROSS) || (g_menu_input_newly_pressed & PSX_PAD_CIRCLE)) {
            g_formation_unit_browse_enabled = 0;
            g_formation_unit_banner_enabled = 0;
            g_formation_unit_count
                = formation_record_list_build(0, g_formation_unit_pointers, FORMATION_RECORD_LIST_ALL);
            g_unit_selected_index = g_formation_item_saved_selected_unit_index;
            formation_stage_selected_unit();
            menu_request_thread_stop(THREAD_SCREEN_SLOT_CHARACTER_STATUS);
            menu_request_thread_stop(THREAD_SCREEN_SLOT_NUMERIC_PANEL);
            menu_request_thread_stop(THREAD_SCREEN_SLOT_UNIT_STATUS);
            g_formation_item_equipping_units_initialized = 0;
            g_formation_unit_cycle_mode = 0;
            g_sound_queued_effect_id = SOUND_SFX_CANCEL;
            return MENU_ITEM_LIST_STATE_BROWSE;
        }
    }
    return MENU_ITEM_LIST_STATE_EQUIPPED_BY;
}

/* Draw the Items screen's category tab row (the wide layout when `wide` is
 * set), with tab `row` selected and the tab cursor beside it; `mode` is the
 * script palette mode. The incoming mode stays in a2 until the first
 * drawing-helper call. */
void screen_item_tabs_draw(s32 wide, s32 row, s32 mode) {
    s16 x;
    s16 y;
    /* One pointer serves the script then the quad; separate locals remove the argument copy. */
    void* drawing;
    s32 cursor_x;
    oriented_quad_t* cursor;

    x = g_formation_item_category_tab_x[row];
    if (wide != 0) {
        x -= 1;
    }
    if (wide == 0) {
        drawing = g_formation_item_category_tabs_script;
        y = 0x40;
    } else {
        y = 0x4F;
        drawing = g_formation_item_category_tabs_wide_script;
    }
    menu_run_script_with_palette_mode(drawing, 0, mode);
    drawing = &g_formation_item_category_tab_sprites[row];
    ((oriented_quad_t*)drawing)->position.x = x;
    ((oriented_quad_t*)drawing)->position.y = y - (row == 0);
    menu_gfx_enqueue_oriented_textured_quad(drawing, screen_get_draw_color(), TEXTURE_UV_FLIP_NONE, 0, 0x2C);
    cursor = &g_formation_item_category_tab_cursor_sprite;
    cursor_x = x + 9;
    if (row == 3) {
        cursor_x = x + 7;
    }
    cursor->position.x = cursor_x;
    g_formation_item_category_tab_cursor_sprite.position.y = y + 10;
    menu_gfx_enqueue_oriented_textured_quad(cursor, 0, TEXTURE_UV_FLIP_NONE, 0, 0x2D);
}
