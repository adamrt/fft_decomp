#include "fft/data.h"
#include "fft/event.h"
#include "fft/main_sound.h"
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
        g_world_menu_cursor_position = 0;
    }
    if (g_world_input_primary_repeat & PSX_PAD_CROSS) {
        g_world_menu_sound_effect_id = MAIN_SFX_CANCEL;
        if (g_world_shop_fitting_room_item_list_open != 0) {
            g_world_shop_fitting_item_list_close = 1;
        } else {
            g_world_shop_fitting_equip_menu_initialized = 0;
            g_world_shop_menu_step = 6;
        }
    } else if (g_world_shop_fitting_room_item_list_open == 0) {
        prev = g_world_shop_fitting_equip_slot;
        g_world_shop_fitting_equip_slot = world_menu_step_cursor_with_sound(5, 2, g_world_input_primary_repeat, 6);
        if (prev != g_world_shop_fitting_equip_slot) {
            g_world_menu_cursor_position = 0;
        }
        if (g_world_formation_unit_pointers[g_world_formation_selected_unit_index]
                ->equipment[g_world_shop_fitting_equip_slot]
            != 0) {
            value = (g_world_formation_unit_pointers[g_world_formation_selected_unit_index]
                            ->equipment[g_world_shop_fitting_equip_slot]
                        & 0x3ff)
                + 0x6800;
        } else {
            value = -1;
        }
        g_world_menu_description_text_id = value;
        if (g_world_input_primary_repeat & PSX_PAD_CIRCLE) {
            g_world_menu_sound_effect_id = MAIN_SFX_CONFIRM;
            g_world_formation_unit_cycle_mode = 1;
            g_world_shop_fitting_room_item_list_open = 1;
            g_world_shop_fitting_room_previewed_cursor = -1;
            g_world_shop_cost_window_visible = 0;
            g_world_change_banner_panel_thread_params.style = 1;
            world_shop_build_item_list(g_world_formation_selected_unit_index, g_world_shop_id,
                g_world_shop_fitting_equip_slot, (world_item_list_entry_t*)g_world_menu_entry_ids, 1);
            slot = 0;
            if (g_world_shop_fitting_equip_slot != 0) {
                slot = g_world_shop_fitting_equip_slot - 1;
            }
            world_gfx_get_sprite_slot(slot, &x, &y, g_world_menu_entry_ids);
            world_menu_init_scrollable_list(g_world_menu_entry_ids, x, y, g_world_text_item_names);
            g_world_menu_script_callbacks[0] = world_menu_get_entry_item_count_2;
            g_world_menu_script_callbacks[1] = world_menu_get_entry_item_count;
            g_world_menu_aux_callback = (s32 (*)(void))world_item_build_type_icon_source;
            g_world_menu_script_callbacks[3] = (s32 (*)(s32))world_menu_get_entry_item_icon_source;
            g_world_menu_script_callbacks[4] = world_menu_get_item_price_display_value;
            world_script_set_vsync_mode_and_event_speed(2);
        }
    } else {
        item = g_world_menu_entry_ids[g_world_menu_cursor_position];
        if (g_world_input_primary_repeat & PSX_PAD_CIRCLE) {
            if (g_world_menu_option_count == 0 || (item & 0x4000)) {
                g_world_menu_sound_effect_id = MAIN_SFX_INVALID;
            } else {
                equip = item;
                result = world_formation_can_equip_item_in_slot(
                    g_world_formation_selected_unit_index, g_world_shop_fitting_equip_slot, equip);
                if (result < 0) {
                    g_world_menu_sound_effect_id = MAIN_SFX_INVALID;
                } else {
                    world_shop_return_unit_equip_to_fitting_room(
                        g_world_formation_selected_unit_index, g_world_shop_fitting_equip_slot);
                    g_world_formation_unit_pointers[g_world_formation_selected_unit_index]
                        ->equipment[g_world_shop_fitting_equip_slot] = item | 0x8000;
                    world_shop_add_fitting_room_cost(world_item_get_price(equip));
                    if (result != 1) {
                        world_shop_return_unit_equip_to_fitting_room(
                            g_world_formation_selected_unit_index, g_world_shop_fitting_equip_slot == 0);
                    }
                    g_world_shop_fitting_item_list_close = 1;
                    g_world_menu_sound_effect_id = MAIN_SFX_EQUIP;
                    world_formation_recalculate_selected_unit_stats();
                }
            }
        }
        if (g_world_menu_option_count != 0
            && g_world_shop_fitting_room_previewed_cursor != g_world_menu_cursor_position) {
            g_world_shop_fitting_room_previewed_cursor = g_world_menu_cursor_position;
            result = world_formation_equip_item_to_unit_slot(20, g_world_shop_fitting_equip_slot, (s16)item);
            if (result == 1) {
                world_menu_toggle_preview_stats_window(1);
                g_world_preview_stats_thread_params.redraw_request = result;
                world_item_calculate_equipment_swap_stat_delta(&g_world_item_preview_stat_detail,
                    &g_world_selected_unit_stat_summary,
                    g_world_formation_unit_pointers[g_world_formation_selected_unit_index]->equipment,
                    g_world_formation_temp_unit->equipment);
                bcopy(g_world_formation_unit_pointers[g_world_formation_selected_unit_index]->equipment,
                    g_world_formation_temp_unit->equipment, 10);
            } else {
                world_menu_toggle_preview_stats_window(0);
            }
            g_world_formation_panel_windows[0].enabled = 1;
        }
        world_menu_run_script_with_palette_mode(
            g_world_shop_fitting_item_list_menu_script, g_world_input_primary_repeat, world_map_is_busy());
        g_world_menu_description_text_id = g_world_menu_option_count != 0
            ? (g_world_menu_entry_ids[g_world_menu_cursor_position] & 0x3ff) + 0x6800
            : -1;
    }
    if (g_world_shop_fitting_item_list_close != 0) {
        g_world_shop_cost_window_visible = 1;
        g_world_shop_fitting_item_list_close = 0;
        g_world_change_banner_panel_thread_params.style = 0;
        g_world_shop_fitting_room_item_list_open = 0;
        g_world_formation_unit_cycle_mode = 2;
        world_script_set_vsync_mode_and_event_speed(0);
        world_menu_toggle_preview_stats_window(0);
        g_world_formation_panel_windows[0].enabled = 1;
        slot = 0;
        if (g_world_shop_fitting_equip_slot != 0) {
            slot = g_world_shop_fitting_equip_slot - 1;
        }
        world_gfx_set_sprite_slot(
            slot, g_world_menu_cursor_position, g_world_menu_scroll_offset, (u16*)g_world_menu_entry_ids);
    }
    cursor_y = &g_world_shop_fitting_slot_cursor_point.y;
    *cursor_y = g_world_shop_fitting_equip_slot * 16 + 0x90;
    world_menu_set_draw_priority(0x3d);
    world_menu_draw_animated_cursor(&g_world_shop_fitting_slot_cursor_point, &g_world_shop_fitting_slot_cursor_anim,
        g_world_shop_fitting_room_item_list_open != 0 || g_world_thread_task_active != 0);
    if (g_world_shop_cost_window_visible != 0) {
        world_shop_install_callbacks_and_run();
    }
}
