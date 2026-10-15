#include "fft/world.h"
#include "psx/libc.h"
#include "psx/pad.h"

/* Shop fitting-room equipment step, the shop twin of world_formation_run_equip_item_menu.
 *
 * With the slot list active, steps the equipment-slot cursor and shows the
 * equipped item's description; confirming opens the shop item list for the
 * slot. In the item list, confirming an equippable item returns the unit's
 * current item to the fitting room, equips the shop item (bit 15 marks it as
 * unpaid), adds its price to the fitting-room cost and recalculates stats,
 * while moving the cursor previews the stat change. Cancel leaves the list,
 * or the step (g_world_shop_menu_step = 6) from the slot list. */
void world_shop_run_fitting_room_equip_menu(void) {
    s16 x;
    s16 y;
    s32 prev;
    s32 slot;
    s32 value;
    u16 item;
    s16 equip;
    s32 result;
    s16* cursor_y;

    if (g_world_shop_fitting_equip_menu_initialized == 0) {
        g_world_shop_fitting_equip_menu_initialized = 1;
        g_world_shop_fitting_room_previewed_cursor = -1;
        g_world_shop_fitting_room_item_list_open = 0;
        g_menu_list_selected_index = 0;
    }
    if (g_menu_input_primary_repeat & PSX_PAD_CROSS) {
        g_sound_queued_effect_id = MAIN_SFX_CANCEL;
        if (g_world_shop_fitting_room_item_list_open != 0) {
            g_world_shop_fitting_item_list_close = 1;
        } else {
            g_world_shop_fitting_equip_menu_initialized = 0;
            g_world_shop_menu_step = WORLD_SHOP_STEP_FITTING_ROOM;
        }
    } else if (g_world_shop_fitting_room_item_list_open == 0) {
        prev = g_world_shop_fitting_equip_slot;
        g_world_shop_fitting_equip_slot = menu_update_vertical_selection_and_mark_change(WORLD_EQUIPMENT_SLOT_COUNT,
            WORLD_MENU_CURSOR_EQUIP_SLOT, g_menu_input_primary_repeat, MAIN_SFX_PAGE_SWITCH);
        if (prev != g_world_shop_fitting_equip_slot) {
            g_menu_list_selected_index = 0;
        }
        if (g_world_unit_pointers[g_unit_selected_index]->equipment[g_world_shop_fitting_equip_slot]
            != ITEM_ID_NOTHING) {
            value = (g_world_unit_pointers[g_unit_selected_index]->equipment[g_world_shop_fitting_equip_slot]
                        & WORLD_ITEM_ID_MASK)
                + 0x6800;
        } else {
            value = -1;
        }
        g_menu_help_message_text_id = value;
        if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
            g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
            g_world_formation_unit_cycle_mode = 1;
            g_world_shop_fitting_room_item_list_open = 1;
            g_world_shop_fitting_room_previewed_cursor = -1;
            g_world_shop_cost_window_visible = 0;
            g_world_change_banner_panel_thread_params.style = 1;
            world_shop_build_item_list(g_unit_selected_index, g_world_menu_context_id, g_world_shop_fitting_equip_slot,
                g_item_list_entries, ITEM_LIST_MODE_MARK_UNEQUIPPABLE);
            slot = 0;
            if (g_world_shop_fitting_equip_slot != 0) {
                slot = g_world_shop_fitting_equip_slot - 1;
            }
            world_menu_get_selection_record(slot, &x, &y, g_item_list_entries);
            world_menu_init_scrollable_list(
                g_item_list_entries, x, y, g_text_section_pointers[TEXT_SECTION_ITEM_NAMES]);
            g_world_menu_script_callbacks[0].value = menu_get_entry_item_equipped_count;
            g_world_menu_script_callbacks[1].value = menu_get_entry_item_count;
            g_world_menu_script_callbacks[2].icon = menu_build_item_type_icon_source;
            g_world_menu_script_callbacks[3].icon = menu_get_entry_item_icon_source;
            g_world_menu_script_callbacks[4].value = world_menu_get_item_price_display_value;
            menu_set_vsync_mode_and_event_speed(2);
        }
    } else {
        item = g_item_list_entries[g_menu_list_selected_index];
        if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
            if (g_menu_list_entry_count == 0 || (item & WORLD_ITEM_LIST_ENTRY_UNEQUIPPABLE)) {
                g_sound_queued_effect_id = MAIN_SFX_INVALID;
            } else {
                equip = item;
                result = world_formation_can_equip_item_in_slot(
                    g_unit_selected_index, g_world_shop_fitting_equip_slot, equip);
                if (result < 0) {
                    g_sound_queued_effect_id = MAIN_SFX_INVALID;
                } else {
                    world_shop_return_unit_equip_to_fitting_room(
                        g_unit_selected_index, g_world_shop_fitting_equip_slot);
                    g_world_unit_pointers[g_unit_selected_index]->equipment[g_world_shop_fitting_equip_slot]
                        = item | WORLD_EQUIPMENT_SHOP_ITEM;
                    world_shop_add_fitting_room_cost(world_item_get_price(equip));
                    if (result != 1) {
                        world_shop_return_unit_equip_to_fitting_room(
                            g_unit_selected_index, g_world_shop_fitting_equip_slot == 0);
                    }
                    g_world_shop_fitting_item_list_close = 1;
                    g_sound_queued_effect_id = MAIN_SFX_EQUIP;
                    world_formation_recalculate_selected_unit_stats();
                }
            }
        }
        if (g_menu_list_entry_count != 0 && g_world_shop_fitting_room_previewed_cursor != g_menu_list_selected_index) {
            g_world_shop_fitting_room_previewed_cursor = g_menu_list_selected_index;
            result = world_formation_equip_item_to_unit_slot(
                WORLD_FORMATION_PREVIEW_UNIT_INDEX, g_world_shop_fitting_equip_slot, item);
            if (result == 1) {
                world_menu_toggle_preview_stats_window(1);
                g_menu_preview_stats_thread_params.redraw_request = result;
                item_calculate_equipment_swap_stat_delta(&g_item_preview_stat_detail,
                    &g_menu_selected_unit_stat_summary, g_world_unit_pointers[g_unit_selected_index]->equipment,
                    g_world_formation_temp_unit->equipment);
                bcopy(g_world_unit_pointers[g_unit_selected_index]->equipment, g_world_formation_temp_unit->equipment,
                    10);
            } else {
                world_menu_toggle_preview_stats_window(0);
            }
            g_menu_status_display_thread_params.redraw_request = 1;
        }
        menu_run_script_with_palette_mode(
            &g_world_shop_fitting_item_list_menu_script, g_menu_input_primary_repeat, world_map_is_busy());
        g_menu_help_message_text_id = g_menu_list_entry_count != 0
            ? (g_item_list_entries[g_menu_list_selected_index] & WORLD_ITEM_ID_MASK) + 0x6800
            : -1;
    }
    if (g_world_shop_fitting_item_list_close != 0) {
        g_world_shop_cost_window_visible = 1;
        g_world_shop_fitting_item_list_close = 0;
        g_world_change_banner_panel_thread_params.style = 0;
        g_world_shop_fitting_room_item_list_open = 0;
        g_world_formation_unit_cycle_mode = 2;
        menu_set_vsync_mode_and_event_speed(0);
        world_menu_toggle_preview_stats_window(0);
        g_menu_status_display_thread_params.redraw_request = 1;
        slot = 0;
        if (g_world_shop_fitting_equip_slot != 0) {
            slot = g_world_shop_fitting_equip_slot - 1;
        }
        menu_set_selection_record(slot, g_menu_list_selected_index, g_menu_scroll_base_index, g_item_list_entries);
    }
    cursor_y = &g_world_shop_fitting_slot_cursor_point.y;
    *cursor_y = g_world_shop_fitting_equip_slot * 16 + 0x90;
    world_menu_set_draw_priority(0x3d);
    world_menu_draw_animated_cursor(&g_world_shop_fitting_slot_cursor_point, &g_world_shop_fitting_slot_cursor_anim,
        g_world_shop_fitting_room_item_list_open != 0 || g_menu_input_lock_mode != 0);
    if (g_world_shop_cost_window_visible != 0) {
        world_shop_install_callbacks_and_run();
    }
}
