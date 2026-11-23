/* Run one frame of the battle Equip screen. Selected-unit and message state
 * coordinate its command, equipment and numeric-panel threads through closing. */

#include "fft/game/battle_unit.h"
#include "fft/menu/help.h"
#include "fft/menu/menu_gfx.h"
#include "fft/menu/menu_input.h"
#include "fft/menu/menu_list.h"
#include "fft/menu/menu_text.h"
#include "fft/party/equipment.h"
#include "fft/party/unit_panel.h"
#include "fft/sys/sound.h"
#include "fft/sys/thread.h"
#include "fft/text/message.h"
#include "fft/text/text.h"
#include "psx/libc.h"
#include "psx/pad.h"
#include "psx/types.h"

extern s16 g_equipment_unit_initial_equipment[];
static u8 s_equipment_text_restore_pending;

/* Start the item numeric panel thread when `enable` is set and the thread is
 * idle, ask it to stop otherwise, and record the state in
 * g_equipment_item_numeric_thread_enabled and the numeric panel style. */
void unit_panel_toggle_preview_stats_thread(s32 enable) {
    if (enable != 0) {
        if (thread_is_running(EQUIPMENT_THREAD_SLOT_ITEM_NUMERIC) == 0) {
            thread_start(EQUIPMENT_THREAD_SLOT_ITEM_NUMERIC, unit_panel_numbers_thread);
            thread_set_parameters(
                EQUIPMENT_THREAD_SLOT_ITEM_NUMERIC, (s32)&g_unit_panel_preview_stats_thread_params, 0, 0);
        }
    } else {
        menu_thread_request_stop(EQUIPMENT_THREAD_SLOT_ITEM_NUMERIC);
    }
    g_equipment_item_numeric_thread_enabled = enable;
    g_unit_panel_numeric_panel_params.style = enable;
}

/* Prime the four "menu section dirty" flags before the
 * next redraw pass. */
static void redraw_unit_panels(void) {
    g_unit_panel_unit_status_panel_params.redraw_request = 1;
    g_unit_panel_numeric_panel_params.redraw_request = 1;
    g_unit_panel_preview_stats_thread_params.redraw_request = 1;
    g_equipment_panel_params.redraw_request = 1;
}

/* Ask the unit status, equipment list, numeric panel and item numeric
 * panel threads to stop and clear the two panel styles. */
void unit_panel_stop_all(void) {
    menu_thread_request_stop(EQUIPMENT_THREAD_SLOT_UNIT_STATUS);
    menu_thread_request_stop(EQUIPMENT_THREAD_SLOT_EQUIPMENT_LIST);
    menu_thread_request_stop(THREAD_SCREEN_SLOT_NUMERIC_PANEL);
    unit_panel_toggle_preview_stats_thread(0);
    g_unit_panel_preview_stats_thread_params.style = 0;
    g_equipment_panel_params.style = 0;
}

/* Load the selected unit into the EQUIP panels: clamp its HP and MP to their
 * maximums, copy its stat summary, identity and stat detail blocks, add the
 * equipment's stat details, copy the unit to compare slot 1 and reset the
 * menu state flags. */
void equipment_refresh_selected_unit(void) {
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
    equipment_build_weapon_evade_totals(
        &g_unit_panel_selected_unit_fields, g_equipment_unit_data[g_equipment_unit_selected_index]->equipment);
    equipment_copy_unit_to_comparison(g_equipment_unit_selected_index, 1);
    redraw_unit_panels();
}

/* Fill the 4 s16 selection-index slots ending at
 * g_equipment_result_rows with -1 (writing backwards). */
void equipment_reset_selection_results(void) {
    s16 sentinel = -1;
    s32 remaining;

    for (remaining = 3; remaining >= 0; remaining--) {
        g_equipment_result_rows[remaining] = sentinel;
    }
}

/* Show message `message_id` in the text thread (dialog type 0x2B), play
 * `sound_id` at once with the queued sound suppressed, and lock the menu
 * input. */
void message_box_show_with_sound(s32 message_id, s32 sound_id) {
    menu_text_thread_start_if_idle(THREAD_SCREEN_SLOT_TEXT, 0x2B, message_id, 0);
    g_equipment_sound_suppress_queued = 1;
    sound_play_sfx(sound_id);
    g_equipment_text_message_thread_active = 1;
    g_menu_input_lock_mode = 1;
}

/* Run one frame of the equipment screen. Returns the frame's ordering table
 * while the screen is open, then whether the unit's equipment changed once it
 * closes. */
s32 equip_entrypoint(s32 unit, u32* otag) {
    s32 result;
    s32 i;
    u8 restore_text;
    battle_unit_t* stats;

    if (g_equipment_screen_state == EQUIPMENT_STATE_INIT) {
        equipment_init(unit);
        bcopy(g_equipment_unit_data[0]->equipment, g_equipment_unit_initial_equipment,
            sizeof(g_equipment_unit_data[0]->equipment));
        g_equipment_screen_state = EQUIPMENT_STATE_COMMAND_MENU;
    }
    g_equipment_gfx_current_ot = otag;
    menu_gfx_swap_context(0, -1);
    result = (s32)g_menu_gfx_context->otag;
    /* While input is locked by text other than an EQUIP message (the SELECT help), the
     * text sections are restored once the lock ends. */
    restore_text = 0;
    if (g_menu_input_lock_mode != 0) {
        restore_text = g_equipment_text_message_thread_active == 0;
    }
    s_equipment_text_restore_pending = restore_text;
    menu_input_update_message_state();
    if (g_menu_input_primary_repeat & PSX_PAD_SELECT) {
        help_start_text(3);
    }
    if (s_equipment_text_restore_pending != 0) {
        if (g_menu_input_lock_mode == 0) {
            text_restore_section_pointers();
        }
    }
    menu_palette_set_mode(g_menu_input_lock_mode);
    g_equipment_screen_state_handlers[g_equipment_screen_state]();
    if (g_equipment_sound_suppress_queued != 0) {
        g_sound_queued_effect_id = SOUND_SFX_NONE;
    }
    if (g_sound_queued_effect_id != SOUND_SFX_NONE) {
        g_sound_pending_sfx_id = g_sound_queued_effect_id;
    }
    g_sound_queued_effect_id = SOUND_SFX_NONE;
    if (g_equipment_screen_state != EQUIPMENT_STATE_CLOSED) {
        return result;
    }

    /* Closed: report whether any of the five slots changed, after clamping HP and MP to the
     * new maximums and refreshing the battle unit. */
    for (i = 0; i < 5; i++) {
        if (g_equipment_unit_data[0]->equipment[i] != g_equipment_unit_initial_equipment[i]) {
            break;
        }
    }
    stats = battle_unit_get(unit);
    if (stats->max_hp < stats->hp) {
        stats->hp = stats->max_hp;
    }
    if (stats->max_mp < stats->mp) {
        stats->mp = stats->max_mp;
    }
    battle_unit_refresh_stats(stats);
    return i < 5;
}
