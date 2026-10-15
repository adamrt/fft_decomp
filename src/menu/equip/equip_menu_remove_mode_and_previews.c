/* Remove mode, clear-all prompt and the previewed HP/MP bonuses. */

#include "fft/event_equip.h"
#include "fft/menu.h"
#include "psx/libc.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Runs the equipment-slot list that removes equipment.
 *
 * On entry it snapshots the unit's five items. Cancel leaves the mode, the
 * 0x8000 button opens the remove-all prompt, confirm unequips the highlighted
 * slot, and moving the cursor previews the stats without that item on the
 * scratch unit record g_equip_unit_data[1]. */
s32 equip_menu_run_remove_mode(void) {
    /* Never referenced; the target frame reserves these bytes. */
    u8 unused[0x48];
    s32 i;
    u32 input;
    s32 selection;
    u8 has_item;

    if (g_equip_remove_mode_initialized == 0) {
        for (i = 0; i < 5; i++) {
            g_remove_item_equipment[i] = g_equip_unit_data[g_equip_unit_selected_index]->equipment[i];
        }
        g_remove_item_previewed_slot = -1;
        g_remove_all_equipment_prompt_active = 0;
        g_equip_remove_mode_initialized = 1;
    }
    input = g_menu_input_primary_repeat;
    if (input & PSX_PAD_CROSS) {
        equip_panel_toggle_item_numeric_thread(0);
        g_equip_unit_status_panel_redraw = 1;
        g_equip_remove_mode_initialized = 0;
        g_sound_queued_effect_id = MAIN_SFX_CANCEL;
        return 0;
    }
    if (g_remove_all_equipment_prompt_active != 0) {
        g_remove_all_equipment_prompt_active = equip_menu_update_clear_all_equipment_prompt();
        if (g_remove_all_equipment_prompt_active == 0) {
            g_remove_item_previewed_slot = -2;
            bcopy(g_equip_unit_data[g_equip_unit_selected_index]->equipment, g_remove_item_equipment, 10);
        }
        return 1;
    }
    if (input & PSX_PAD_LEFT) {
        g_remove_all_equipment_prompt_active = 1;
        item_calculate_equipment_swap_stat_delta(&g_item_preview_stat_detail, &g_menu_selected_unit_stat_summary,
            g_equip_unit_data[g_equip_unit_selected_index]->equipment, g_remove_all_equipment_ids);
        g_equip_unit_status_panel_redraw = 1;
        g_menu_preview_stats_thread_params.redraw_request = 1;
        equip_panel_toggle_item_numeric_thread(1);
        g_sound_queued_effect_id = MAIN_SFX_PAGE_SWITCH;
    }
    selection
        = menu_update_vertical_selection_and_mark_change(5, 1, g_menu_input_secondary_repeat, MAIN_SFX_PAGE_SWITCH);
    g_equip_remove_item_marker_rect.y = ((selection & 0xFF) << 4) + 0x90;
    gfx_set_otag_index(0x28);
    menu_update_and_draw_animated_cursor(
        &g_equip_remove_item_marker_rect, g_equip_remove_item_cursor_anim, g_menu_input_lock_mode);

    g_menu_help_message_text_id
        = g_remove_item_equipment[selection & 0xFF] != 0 ? g_remove_item_equipment[selection & 0xFF] + 0x6800 : -1;
    if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
        if (g_remove_item_equipment[selection & 0xFF] == 0
            || equip_unit_set_slot_item(g_equip_unit_selected_index, (u8)selection, 0)
                == EQUIP_SLOT_ITEM_CURRENT_LOCKED) {
            g_sound_queued_effect_id = MAIN_SFX_INVALID;
        } else {
            bcopy(g_equip_unit_data[g_equip_unit_selected_index]->equipment, g_remove_item_equipment, 10);
            equip_unit_load_selected_data();
            g_sound_queued_effect_id = MAIN_SFX_UNEQUIP;
            g_remove_item_previewed_slot = -1;
        }
    }
    if (g_remove_item_previewed_slot != (selection & 0xFF)) {
        g_remove_item_previewed_slot = selection;
        has_item = g_remove_item_equipment[selection & 0xFF];
        if (has_item != 0) {
            equip_unit_set_slot_item(1, (u8)selection, 0);
            item_calculate_equipment_swap_stat_delta(&g_item_preview_stat_detail, &g_menu_selected_unit_stat_summary,
                g_equip_unit_data[g_equip_unit_selected_index]->equipment, g_equip_unit_data[1]->equipment);
            bcopy(g_equip_unit_data[g_equip_unit_selected_index]->equipment, g_equip_unit_data[1]->equipment, 10);
            g_menu_preview_stats_thread_params.redraw_request = 1;
        }
        g_equip_unit_status_panel_redraw = 1;
        equip_panel_toggle_item_numeric_thread(has_item != 0);
    }
    return 1;
}

/*
 * Update the clear-all-equipment prompt and empty every occupied slot when accepted.
 *
 * Returns 1 while waiting for input and 0 after the prompt is accepted or dismissed.
 */
s32 equip_menu_update_clear_all_equipment_prompt(void) {
    u8 unused_buffer[0x40]; /* unreferenced; reserves the target's frame */
    u16 marker_state;
    s32 index;
    s32 unequipped;
    s32 flags;

    g_menu_help_message_text_id = -1;
    gfx_set_otag_index(0x28);
    g_equip_menu_clear_all_prompt_frame = g_equip_menu_clear_all_prompt_frame + 1;
    if (g_equip_menu_clear_all_prompt_frame & 1) {
        for (index = 0; index < 5; index++) {
            g_equip_menu_clear_all_prompt_draw_params.y = 0x90 + index * 0x10;
            marker_state = 0;
            menu_update_and_draw_animated_cursor(
                &g_equip_menu_clear_all_prompt_draw_params, &marker_state, g_menu_input_lock_mode);
        }
    }
    flags = g_menu_input_primary_repeat;
    if (flags & PSX_PAD_RIGHT) {
        g_sound_queued_effect_id = MAIN_SFX_PAGE_SWITCH;
        return 0;
    }
    if (flags & PSX_PAD_CIRCLE) {
        unequipped = 0;
        for (index = 0; index < 5; index++) {
            if (g_equip_unit_data[g_equip_unit_selected_index]->equipment[index] != 0) {
                unequipped = 1;
                equip_unit_set_slot_item(g_equip_unit_selected_index, index, 0);
            }
        }
        equip_unit_load_selected_data();
        if (unequipped == 0) {
            g_sound_queued_effect_id = MAIN_SFX_INVALID;
        } else {
            g_sound_queued_effect_id = MAIN_SFX_UNEQUIP;
        }
        return 0;
    }
    return 1;
}

s32 equip_menu_get_preview_hp_bonus(s32 unused_row) {
    return g_menu_selected_unit_stat_summary.gauges[MENU_UNIT_STATUS_GAUGE_HP].bonus;
}

s32 equip_menu_get_preview_mp_bonus(s32 unused_row) {
    return g_menu_selected_unit_stat_summary.gauges[MENU_UNIT_STATUS_GAUGE_MP].bonus;
}
