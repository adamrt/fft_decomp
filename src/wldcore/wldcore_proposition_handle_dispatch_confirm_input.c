#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Input step of the proposition-dispatch yes/no panel built by wldcore_window_build_yes_no_panel:
 * up/down toggle the row, cancel (0x40) and "no" close the panel; "yes" pays
 * g_wldcore_proposition_gil_amount from script variable 0x2c, marks the proposition taken, records
 * it in the next g_main_active_propositions slot with its participants, and
 * closes the panel. */
void wldcore_proposition_handle_dispatch_confirm_input(wldcore_menu_choice_panel_level_t* level) {
    s32 row;
    s32 i;

    if (g_wldcore_window_render_records[level->render_index].flags & WORLD_RENDER_RECORD_POP_IN) {
        return;
    }
    if (g_wldcore_input_new_button_presses & PSX_PAD_CROSS) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        /* Shared tail: the target keeps this copy and both confirm paths
         * jump into it; duplicated copies cross-jump into the last one. */
    block_tail:
        g_wldcore_context_value_display_mode = WLDCORE_VALUE_DISPLAY_FUNDS;
        g_wldcore_window_record_count -= 2;
        g_wldcore_window_render_record_count -= 2;
        g_wldcore_window_render_object_count -= 4;
        g_wldcore_menu_stack_depth -= 1;
        wldcore_list_open_available_propositions();
        return;
    }
    if (wldcore_input_update_repeat(PSX_PAD_UP) != 0 || wldcore_input_update_repeat(PSX_PAD_DOWN) != 0) {
        row = level->row ^ 1;
        level->row = row;
        g_wldcore_window_records[level->cursor_window].position.y
            = g_wldcore_window_render_records[level->render_index].base.y - -(row * 0x10 + 0xE);
        wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
    }
    if (g_wldcore_input_new_button_presses & PSX_PAD_CIRCLE) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        if (level->row != 1) {
            script_set_variable(EVENT_SCRIPT_VAR_WAR_FUNDS,
                script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS) - g_wldcore_proposition_gil_amount);
            g_wldcore_displayed_numeric_value = script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS);
            script_set_variable(g_wldcore_selected_proposition_row[0].id + (EVENT_SCRIPT_VAR_PROPOSITION_FIRST - 1),
                script_get_variable(g_wldcore_selected_proposition_row[0].id + (EVENT_SCRIPT_VAR_PROPOSITION_FIRST - 1))
                    | WLDCORE_PROPOSITION_VARIABLE_ATTEMPTED);
            g_main_active_propositions[g_main_save_proposition_count].flags = WLDCORE_PROPOSITION_PROGRESS_DISPATCHED;
            g_main_active_propositions[g_main_save_proposition_count].proposition_id
                = g_wldcore_selected_proposition_row[0].id - 1;
            g_main_active_propositions[g_main_save_proposition_count].elapsed_days = 0;
            g_main_active_propositions[g_main_save_proposition_count].assigned_days
                = g_wldcore_proposition_dispatch_days;
            g_main_active_propositions[g_main_save_proposition_count].location
                = g_wldcore_map_projection_state.marker.kind;
            g_main_active_propositions[g_main_save_proposition_count].participant_count
                = g_wldcore_proposition_send_unit_count;
            for (i = 0; i < g_wldcore_proposition_send_unit_count; i++) {
                wldcore_get_party_unit(g_wldcore_proposition_send_units[i])->proposition_status = 1;
                g_main_active_propositions[g_main_save_proposition_count].participant_indices[i]
                    = g_wldcore_proposition_send_units[i];
            }
            g_main_save_proposition_count++;
            script_set_variable(g_wldcore_selected_proposition_row[0].id + (EVENT_SCRIPT_VAR_PROPOSITION_FIRST - 1),
                script_get_variable(g_wldcore_selected_proposition_row[0].id + (EVENT_SCRIPT_VAR_PROPOSITION_FIRST - 1))
                    | WLDCORE_PROPOSITION_VARIABLE_IN_PROGRESS);
        }
        goto block_tail;
    }
}
