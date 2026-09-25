#include "fft/world.h"
#include "psx/libc.h"
#include "psx/pad.h"

/* Run the formation "Equip" item menu: step the slot cursor, open the item
 * list for the selected slot, equip the highlighted item and keep the stat
 * preview in sync. Returns 0 when the menu is cancelled, 1 otherwise.
 */
s32 world_formation_run_equip_item_menu(void) {
    s16 x;
    s16 y;
    u8 locked;
    s32 slot;
    s32 item;
    /* s16: the conversion from item keeps CSE from folding the copy away
       (the target moves item into a second saved register here). */
    s16 equip;
    u16 equipped;
    s32 value;
    s16* cursor_y;

    /* The target tests the init flag with lb; the header types it u8. */
    if (*(s8*)&g_world_equip_menu_initialized == 0) {
        g_world_equip_menu_initialized = 1;
        g_world_equip_item_previewed_cursor = -1;
        g_world_equip_item_list_open = 0;
        g_world_formation_unit_cycle_mode = 2;
        g_world_equip_item_slot = 0;
        g_world_equip_item_list_refresh = 0;
        g_world_equip_item_preview_active = 0;
    }
    if (g_world_input_primary_repeat & PSX_PAD_CROSS) {
        g_world_menu_sound_effect_id = MAIN_SFX_CANCEL;
        if (g_world_equip_item_list_open == 0) {
            g_world_equip_menu_initialized = 0;
            world_menu_reset_selection_results();
            return 0;
        }
        g_world_equip_item_list_refresh = 1;
    }
    locked = world_formation_lock_equipment_and_abilities(g_world_formation_selected_unit_index);
    if (locked == 1) {
        locked = 0;
    }
    if (locked != 0) {
        world_menu_toggle_preview_stats_window(0);
        g_world_menu_description_text_id = 0;
        if (g_world_input_primary_repeat & PSX_PAD_CIRCLE) {
            g_world_menu_sound_effect_id = MAIN_SFX_INVALID;
        }
        return 1;
    }
    if (g_world_equip_item_list_open == 0) {
        g_world_equip_item_slot = world_menu_step_cursor_with_sound(5, 2, g_world_input_secondary_repeat, 6);
        if (g_world_input_primary_repeat & PSX_PAD_CIRCLE) {
            /* Called with an s16 second parameter: the target passes the
               slot + 1 unmasked, and the separate s8 conversion of the third
               argument is what gives the two sra of one sll. */
            ((s32 (*)(s16, s16, s8, world_item_list_entry_t*, u8))world_item_build_category_list)(
                g_world_formation_selected_unit_index, g_world_equip_item_slot + 1, g_world_equip_item_slot,
                (world_item_list_entry_t*)g_world_menu_entry_ids, 1);
            world_script_set_vsync_mode_and_event_speed(2);
            g_world_formation_unit_cycle_mode = 1;
            g_world_equip_item_list_open = 1;
            g_world_change_banner_panel_thread_params.style = 1;
            world_thread_suspend(0xf);
            slot = 0;
            if (g_world_equip_item_slot != 0) {
                slot = g_world_equip_item_slot - 1;
            }
            world_gfx_get_sprite_slot(slot, &x, &y, g_world_menu_entry_ids);
            world_menu_init_scrollable_list(g_world_menu_entry_ids, x, y, g_world_text_item_names);
            g_world_menu_script_callbacks[0] = world_menu_get_entry_item_count_2;
            g_world_menu_script_callbacks[1] = world_menu_get_entry_item_count;
            g_world_menu_aux_callback = (s32 (*)(void))world_item_build_type_icon_source;
            g_world_menu_script_callbacks[3] = (s32 (*)(s32))world_menu_get_entry_item_icon_source;
            g_world_menu_sound_effect_id = MAIN_SFX_CONFIRM;
            g_world_equip_item_previewed_cursor = -1;
        }
        equipped = g_world_formation_unit_pointers[g_world_formation_selected_unit_index]
                       ->equipment[g_world_equip_item_slot];
        value = -1;
        if (equipped != 0) {
            value = equipped + 0x6800;
        }
        g_world_menu_description_text_id = value;
    } else {
        g_world_menu_description_text_id = g_world_menu_option_count != 0
            ? (g_world_menu_entry_ids[g_world_menu_cursor_position] & 0x3ff) + 0x6800
            : -1;
        item = g_world_menu_option_count != 0 ? g_world_menu_entry_ids[g_world_menu_cursor_position] & 0x3ff : 0;
        if (g_world_input_primary_repeat & PSX_PAD_CIRCLE) {
            if (g_world_menu_option_count == 0 || (g_world_menu_entry_ids[g_world_menu_cursor_position] & 0x4000)) {
                g_world_menu_sound_effect_id = MAIN_SFX_INVALID;
            } else {
                equip = item;
                if (world_item_change_quantity_on_equip(equip, 0) == 0) {
                    world_text_show_message_and_play_sound(0xc003, 0x30);
                } else if (world_formation_equip_item_to_unit_slot(
                               g_world_formation_selected_unit_index, g_world_equip_item_slot, equip)
                    == 1) {
                    world_item_change_quantity_on_equip(equip, -1);
                    world_formation_stage_selected_unit();
                    g_world_equip_item_list_refresh = 1;
                    g_world_menu_sound_effect_id = MAIN_SFX_EQUIP;
                } else {
                    g_world_menu_sound_effect_id = MAIN_SFX_INVALID;
                }
            }
        }
        if (g_world_menu_option_count != 0 && g_world_equip_item_previewed_cursor != g_world_menu_cursor_position) {
            g_world_equip_item_previewed_cursor = g_world_menu_cursor_position;
            if (world_formation_equip_item_to_unit_slot(20, g_world_equip_item_slot, item) == 1) {
                g_world_equip_item_preview_active = 1;
                g_world_preview_stats_thread_params.redraw_request = 1;
                world_item_calculate_equipment_swap_stat_delta(&g_world_item_preview_stat_detail,
                    &g_world_selected_unit_stat_summary,
                    g_world_formation_unit_pointers[g_world_formation_selected_unit_index]->equipment,
                    g_world_formation_temp_unit->equipment);
                bcopy(g_world_formation_unit_pointers[g_world_formation_selected_unit_index]->equipment,
                    g_world_formation_temp_unit->equipment, 10);
            } else {
                g_world_equip_item_preview_active = 0;
            }
            g_world_formation_panel_windows[0].enabled = 1;
        }
        world_menu_run_display_script(g_world_equip_item_menu_script, g_world_input_primary_repeat);
    }
    if (g_world_equip_item_list_refresh != 0) {
        g_world_change_banner_panel_thread_params.style = 0;
        g_world_equip_item_list_open = 0;
        g_world_formation_unit_cycle_mode = 2;
        world_script_set_vsync_mode_and_event_speed(0);
        g_world_equip_item_preview_active = 0;
        g_world_formation_panel_windows[0].enabled = 1;
        world_thread_resume(0xf);
        slot = 0;
        if (g_world_equip_item_slot != 0) {
            slot = g_world_equip_item_slot - 1;
        }
        world_gfx_set_sprite_slot(
            slot, g_world_menu_cursor_position, g_world_menu_scroll_offset, (u16*)g_world_menu_entry_ids);
        g_world_equip_item_list_refresh = 0;
    }
    world_menu_toggle_preview_stats_window(g_world_equip_item_preview_active);
    cursor_y = &g_world_equip_item_cursor_point.y;
    *cursor_y = g_world_equip_item_slot * 16 + 0x90;
    world_menu_set_draw_priority(0x28);
    /* The mode is an argument expression so &g_world_equip_item_cursor_point is formed before its
       branches and derived from cursor_y (addiu a0,s0,-2), as in the target. */
    world_menu_draw_animated_cursor(&g_world_equip_item_cursor_point, &g_world_equip_item_cursor_anim,
        g_world_equip_item_list_open != 0 || g_world_thread_task_active != 0);
    return 1;
}
