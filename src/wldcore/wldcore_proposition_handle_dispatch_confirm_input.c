#include "fft/data.h"
#include "fft/main_runtime.h"
#include "fft/main_sound.h"
#include "fft/script_variables.h"
#include "fft/wldcore.h"
#include "fft/world.h"
#include "psx/pad.h"

u32 wldcore_input_check_repeating_directional(u32 buttons);
void wldcore_list_open_available_propositions(void);

/* Input step of the proposition-dispatch yes/no panel built by wldcore_window_build_yes_no_panel:
 * up/down toggle the row, cancel (0x40) and "no" close the panel; "yes" pays
 * g_wldcore_proposition_gil_amount from script variable 0x2c, marks the proposition taken, records
 * it in the next g_main_active_propositions slot with its participants, and
 * closes the panel. */
void wldcore_proposition_handle_dispatch_confirm_input(wldcore_menu_choice_panel_level_t* level) {
    s32 row;
    s32 i;
    u16* id;
    u16* id2;
    s32 value;

    if (g_wldcore_window_render_records[level->render_index].flags & 0x100) {
        return;
    }
    if (g_wldcore_new_button_presses & PSX_PAD_CROSS) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        /* Shared tail: the target keeps this copy and both confirm paths
         * jump into it; duplicated copies cross-jump into the last one. */
    block_tail:
        g_wldcore_context_value_display_mode = 1;
        g_wldcore_window_record_count -= 2;
        g_wldcore_window_render_record_count -= 2;
        g_wldcore_window_render_object_count -= 4;
        g_wldcore_menu_stack_depth -= 1;
        wldcore_list_open_available_propositions();
        return;
    }
    if (wldcore_input_check_repeating_directional(PSX_PAD_UP) != 0
        || wldcore_input_check_repeating_directional(PSX_PAD_DOWN) != 0) {
        row = level->row ^ 1;
        level->row = row;
        g_wldcore_window_records[level->cursor_window].y
            = g_wldcore_window_render_records[level->render_index].base_y - -(row * 0x10 + 0xE);
        wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
    }
    if (g_wldcore_new_button_presses & PSX_PAD_CIRCLE) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        if (level->row == 1) {
            goto block_tail;
        }
        id = &g_wldcore_selected_proposition_row[0].fields.id;
        world_script_set_variable(EVENT_SCRIPT_VAR_WAR_FUNDS,
            world_script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS) - g_wldcore_proposition_gil_amount);
        g_wldcore_displayed_numeric_value = world_script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS);
        world_script_set_variable(*id + 0x35F, world_script_get_variable(*id + 0x35F) | 8);
        g_main_active_propositions[g_main_save_proposition_count].flags = 1;
        g_main_active_propositions[g_main_save_proposition_count].proposition_id = (s16)*id - 1;
        g_main_active_propositions[g_main_save_proposition_count].elapsed_days = 0;
        value = g_wldcore_proposition_dispatch_days;
        g_main_active_propositions[g_main_save_proposition_count].assigned_days = value;
        value = g_wldcore_map_projection_state.marker.kind;
        g_main_active_propositions[g_main_save_proposition_count].location = value;
        value = g_wldcore_proposition_send_unit_count;
        g_main_active_propositions[g_main_save_proposition_count].participant_count = value;
        for (i = 0; i < g_wldcore_proposition_send_unit_count; i++) {
            s32 member;

            wldcore_get_party_data_pointer(g_wldcore_proposition_send_units[i])->proposition_status = 1;
            g_main_active_propositions[g_main_save_proposition_count].participant_indices[i] = member
                = g_wldcore_proposition_send_units[i];
        }
        g_main_save_proposition_count++;
        id2 = &g_wldcore_selected_proposition_row[0].fields.id;
        world_script_set_variable(*id2 + 0x35F, world_script_get_variable(*id2 + 0x35F) | 2);
        goto block_tail;
    }
}
