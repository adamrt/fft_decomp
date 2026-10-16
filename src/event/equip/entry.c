/* EQUIP entry, including the item numeric thread toggle, state flags, thread stops, selected unit data, selection
 * indices and messages with sound. */

#include "fft/equipment.h"
#include "fft/menu.h"
#include "fft/menu_gfx.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/unit_panel.h"
#include "fft/unit_runtime.h"
#include "psx/libc.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Start the item numeric panel thread when `enable` is set and the thread is
 * idle, ask it to stop otherwise, and record the state in
 * g_equipment_item_numeric_thread_enabled and the numeric panel style. */
void unit_panel_toggle_item_numbers_thread(s32 enable) {
    if (enable != 0) {
        if (thread_is_running(EQUIPMENT_THREAD_SLOT_ITEM_NUMERIC) == 0) {
            thread_start(EQUIPMENT_THREAD_SLOT_ITEM_NUMERIC, unit_panel_numbers_thread);
            thread_set_parameters(
                EQUIPMENT_THREAD_SLOT_ITEM_NUMERIC, (s32)&g_unit_panel_preview_stats_thread_params, 0, 0);
        }
    } else {
        menu_request_thread_stop(EQUIPMENT_THREAD_SLOT_ITEM_NUMERIC);
    }
    g_equipment_item_numeric_thread_enabled = enable;
    g_equipment_numeric_panel_style = enable;
}

/* Prime the four "menu section dirty" flags before the
 * next redraw pass. */
static void equipment_reset_dirty_flags(void) {
    g_unit_panel_unit_status_panel_redraw = 1;
    g_equipment_numeric_panel_redraw = 1;
    g_unit_panel_preview_stats_thread_params.redraw_request = 1;
    g_equipment_equipment_panel_redraw = 1;
}

/* Ask the unit status, equipment list, numeric panel and item numeric
 * panel threads to stop and clear the two panel styles. */
void equipment_stop_threads(void) {
    menu_request_thread_stop(EQUIPMENT_THREAD_SLOT_UNIT_STATUS);
    menu_request_thread_stop(EQUIPMENT_THREAD_SLOT_EQUIPMENT_LIST);
    menu_request_thread_stop(THREAD_SCREEN_SLOT_NUMERIC_PANEL);
    unit_panel_toggle_item_numbers_thread(0);
    g_unit_panel_preview_stats_thread_params.style = 0;
    g_equipment_equipment_panel_style = 0;
}

/* Load the selected unit into the EQUIP panels: clamp its HP and MP to their
 * maximums, copy its stat summary, identity and stat detail blocks, add the
 * equipment's stat details, copy the unit to compare slot 1 and reset the
 * menu state flags. */
void equipment_load_selected_unit(void) {
    u8 unused[0x80];
    equipment_unit_preview_t* unit;

    unit = g_equipment_unit_data[g_equipment_unit_selected_index];
    if (unit->gauges[MENU_UNIT_STATUS_GAUGE_HP].max < unit->gauges[MENU_UNIT_STATUS_GAUGE_HP].value) {
        unit->gauges[MENU_UNIT_STATUS_GAUGE_HP].value = unit->gauges[MENU_UNIT_STATUS_GAUGE_HP].max;
    }

    unit = g_equipment_unit_data[g_equipment_unit_selected_index];
    if (unit->gauges[MENU_UNIT_STATUS_GAUGE_MP].max < unit->gauges[MENU_UNIT_STATUS_GAUGE_MP].value) {
        unit->gauges[MENU_UNIT_STATUS_GAUGE_MP].value = unit->gauges[MENU_UNIT_STATUS_GAUGE_MP].max;
    }

    bcopy(g_equipment_unit_data[g_equipment_unit_selected_index], &g_unit_panel_selected_unit_stat_summary,
        sizeof(g_unit_panel_selected_unit_stat_summary));
    bcopy(&g_equipment_unit_data[g_equipment_unit_selected_index]->identity, &g_equipment_selected_unit_data,
        sizeof(menu_unit_status_identity_t));
    bcopy(&g_equipment_unit_data[g_equipment_unit_selected_index]->move, &g_unit_panel_selected_unit_fields,
        sizeof(g_unit_panel_selected_unit_fields));
    equipment_sum_stat_details(
        &g_unit_panel_selected_unit_fields, g_equipment_unit_data[g_equipment_unit_selected_index]->equipment);
    equipment_copy_unit_to_comparison(g_equipment_unit_selected_index, 1);
    equipment_reset_dirty_flags();
}

/* Fill the 4 s16 selection-index slots ending at
 * g_equipment_result_rows with -1 (writing backwards). */
void equipment_reset_selection(void) {
    s16 sentinel = -1;
    s32 remaining;

    for (remaining = 3; remaining >= 0; remaining--) {
        g_equipment_result_rows[remaining] = sentinel;
    }
}

/* Show message `message_id` in the text thread (dialog type 0x2B), play
 * `sound_id` at once with the queued sound suppressed, and lock the menu
 * input. */
void equipment_show_message(s32 message_id, s32 sound_id) {
    menu_text_start_thread_if_idle(THREAD_SCREEN_SLOT_TEXT, 0x2B, message_id, 0);
    g_equipment_sound_suppress_queued = 1;
    sound_play_sfx(sound_id);
    g_equipment_text_message_thread_active = 1;
    g_menu_input_lock_mode = 1;
}

/* Run one frame of the equipment screen. Returns the frame's ordering table
 * while the screen is open, then whether the unit's equipment changed once it
 * closes. */
s32 equipment_entrypoint(s32 unit, u32* otag) {
    s32 result;
    s32 i;
    u8 restore_text;
    unit_stats_t* stats;

    if (g_equipment_screen_state == EQUIPMENT_STATE_STATE_INIT) {
        equipment_init(unit);
        bcopy(g_equipment_unit_data[0]->equipment, g_equipment_unit_initial_equipment,
            sizeof(g_equipment_unit_data[0]->equipment));
        g_equipment_screen_state = EQUIPMENT_STATE_STATE_COMMAND_MENU;
    }
    g_equipment_gfx_current_ot = otag;
    equipment_gfx_swap_context(0, -1);
    result = (s32)g_menu_gfx_context->otag;
    /* While input is locked by text other than an EQUIP message (the SELECT help), the
     * text sections are restored once the lock ends. */
    restore_text = 0;
    if (g_menu_input_lock_mode != 0) {
        restore_text = g_equipment_text_message_thread_active == 0;
    }
    g_equipment_text_restore_pending = restore_text;
    menu_input_update_with_message_state();
    if (g_menu_input_primary_repeat & PSX_PAD_SELECT) {
        menu_text_start_help_thread(3);
    }
    if (g_equipment_text_restore_pending != 0) {
        if (g_menu_input_lock_mode == 0) {
            text_restore_section_pointers();
        }
    }
    menu_set_palette_mode(g_menu_input_lock_mode);
    g_equipment_screen_state_handlers[g_equipment_screen_state]();
    if (g_equipment_sound_suppress_queued != 0) {
        g_sound_queued_effect_id = SOUND_SFX_NONE;
    }
    if (g_sound_queued_effect_id != SOUND_SFX_NONE) {
        g_sound_pending_sfx_id = g_sound_queued_effect_id;
    }
    g_sound_queued_effect_id = SOUND_SFX_NONE;
    if (g_equipment_screen_state != EQUIPMENT_STATE_STATE_CLOSED) {
        return result;
    }

    /* Closed: report whether any of the five slots changed, after clamping HP and MP to the
     * new maximums and refreshing the battle unit. */
    for (i = 0; i < 5; i++) {
        if (g_equipment_unit_data[0]->equipment[i] != g_equipment_unit_initial_equipment[i]) {
            break;
        }
    }
    stats = unit_get_stats(unit);
    if (stats->max_hp < stats->hp) {
        stats->hp = stats->max_hp;
    }
    if (stats->max_mp < stats->mp) {
        stats->mp = stats->max_mp;
    }
    unit_refresh_stats_and_statuses(stats);
    return i < 5;
}
