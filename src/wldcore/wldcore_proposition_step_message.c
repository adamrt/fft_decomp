#include "fft/wldcore.h"
#include "psx/pad.h"

/*
 * Queue a proposition message and wait for its acknowledgment.
 *
 * Return 1 and clear the caller's phase after a new button-0x20 press
 * with the menu event flag set. Initial setup and continued waiting return 0.
 */
s32 wldcore_proposition_step_message(wldcore_proposition_message_state_t* state, s32 message) {
    if (state->phase == 0) {
        s32* party_index;
        state->phase = 1;
        if (!world_thread_is_running(14))
            world_thread_start(14, world_text_message_box_thread);
        party_index = &g_wldcore_job_selection.excluded_party_index;
        g_world_text_substitution_values[0] = *party_index + TEXT_ID_UNIT_NAME_BASE;
        world_script_set_variable(
            EVENT_SCRIPT_VAR_DIALOG_PORTRAIT, world_get_party_unit_formation_sprite(*party_index));
        world_thread_set_parameters(
            14, 0x19, wldcore_proposition_adjust_message_index(message, *party_index) + 0x8800, 0);
    } else if (g_wldcore_new_button_presses & PSX_PAD_CIRCLE) {
        if (world_menu_get_event_state_flag()) {
            state->phase = 0;
            return 1;
        }
    }
    return 0;
}
