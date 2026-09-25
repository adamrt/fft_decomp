#include "fft/equip.h"
#include "fft/event.h"
#include "fft/main_sound.h"
#include "fft/world.h"
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
            g_equip_remove_item_equipment[i] = g_equip_unit_data[g_equip_unit_selected_index]->equipment[i];
        }
        g_equip_remove_item_previewed_slot = -1;
        g_equip_remove_all_equipment_prompt_active = 0;
        g_equip_remove_mode_initialized = 1;
    }
    input = g_equip_input_primary_repeat;
    if (input & PSX_PAD_CROSS) {
        equip_panel_toggle_item_numeric_thread(0);
        g_equip_unit_status_panel_redraw = 1;
        g_equip_remove_mode_initialized = 0;
        g_equip_sound_queued_effect_id = MAIN_SFX_CANCEL;
        return 0;
    }
    if (g_equip_remove_all_equipment_prompt_active != 0) {
        g_equip_remove_all_equipment_prompt_active = equip_menu_update_clear_all_equipment_prompt();
        if (g_equip_remove_all_equipment_prompt_active == 0) {
            g_equip_remove_item_previewed_slot = -2;
            bcopy(g_equip_unit_data[g_equip_unit_selected_index]->equipment, g_equip_remove_item_equipment, 10);
        }
        return 1;
    }
    if (input & PSX_PAD_LEFT) {
        g_equip_remove_all_equipment_prompt_active = 1;
        equip_unit_calculate_equipment_swap_stat_deltas(&g_equip_item_preview_stat_detail,
            &g_equip_selected_unit_stat_summary, g_equip_unit_data[g_equip_unit_selected_index]->equipment,
            g_equip_remove_all_equipment_ids);
        g_equip_unit_status_panel_redraw = 1;
        g_equip_item_numeric_panel_redraw = 1;
        equip_panel_toggle_item_numeric_thread(1);
        g_equip_sound_queued_effect_id = MAIN_SFX_PAGE_SWITCH;
    }
    /* The target uses v0 from this void callee. */
    selection = ((s32 (*)(s32, s32, s32, s8))equip_menu_update_vertical_selection_and_mark_change)(
        5, 1, g_equip_input_secondary_repeat, 6);
    g_equip_remove_item_marker_rect.y = ((selection & 0xFF) << 4) + 0x90;
    equip_gfx_set_otag_index(0x28);
    equip_menu_update_and_draw_animated_marker(
        &g_equip_remove_item_marker_rect, g_equip_remove_item_cursor_anim, g_event_mode);

    g_equip_text_help_message_id = g_equip_remove_item_equipment[selection & 0xFF] != 0
        ? g_equip_remove_item_equipment[selection & 0xFF] + 0x6800
        : -1;
    if (g_equip_input_primary_repeat & PSX_PAD_CIRCLE) {
        if (g_equip_remove_item_equipment[selection & 0xFF] == 0
            || equip_unit_set_slot_item(g_equip_unit_selected_index, (u8)selection, 0) == -2) {
            g_equip_sound_queued_effect_id = MAIN_SFX_INVALID;
        } else {
            bcopy(g_equip_unit_data[g_equip_unit_selected_index]->equipment, g_equip_remove_item_equipment, 10);
            equip_unit_load_selected_data();
            g_equip_sound_queued_effect_id = MAIN_SFX_UNEQUIP;
            g_equip_remove_item_previewed_slot = -1;
        }
    }
    if (g_equip_remove_item_previewed_slot != (selection & 0xFF)) {
        g_equip_remove_item_previewed_slot = selection;
        has_item = g_equip_remove_item_equipment[selection & 0xFF];
        if (has_item != 0) {
            equip_unit_set_slot_item(1, (u8)selection, 0);
            equip_unit_calculate_equipment_swap_stat_deltas(&g_equip_item_preview_stat_detail,
                &g_equip_selected_unit_stat_summary, g_equip_unit_data[g_equip_unit_selected_index]->equipment,
                g_equip_unit_data[1]->equipment);
            bcopy(g_equip_unit_data[g_equip_unit_selected_index]->equipment, g_equip_unit_data[1]->equipment, 10);
            g_equip_item_numeric_panel_redraw = 1;
        }
        g_equip_unit_status_panel_redraw = 1;
        equip_panel_toggle_item_numeric_thread(has_item != 0);
    }
    return 1;
}
