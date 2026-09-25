/* The switch jump table lives at .rodata 0x800671dc (0x18 bytes). */
#include "fft/wldcore.h"
#include "psx/pad.h"

/*
 * Run the proposition speaker-selection and message sequence.
 *
 * With speaker == -1 the speaker is drawn at random from the selected
 * proposition's participants other than the excluded one; a proposition with a
 * single participant has nobody else to speak and returns 1 at once. Returns 1
 * once the message has been acknowledged and the phase reset, 0 while running.
 */
s32 wldcore_proposition_step_participant_message(wldcore_proposition_message_state_t* state, s32 message, s32 speaker) {
    switch (state->phase) {
    case 0:
        if (speaker == -1) {
            s32 candidates[5];
            s32 count;
            s32 i;

            if (g_main_active_propositions[g_wldcore_job_selection.proposition_index].participant_count == 1)
                return 1;
            i = 0;
            count = 0;
            if (g_main_active_propositions[g_wldcore_job_selection.proposition_index].participant_count != 0)
                do {
                    s32 participant
                        = g_main_active_propositions[g_wldcore_job_selection.proposition_index].participant_indices[i];
                    if (participant != g_wldcore_job_selection.excluded_party_index)
                        candidates[count++] = participant;
                    i++;
                } while (i < g_main_active_propositions[g_wldcore_job_selection.proposition_index].participant_count);
            g_wldcore_job_selection.speaker_party_index = candidates[(rand() * count) >> 15];
        } else {
            g_wldcore_job_selection.speaker_party_index = speaker;
        }
        world_thread_set_parameters(14, 0, -1, 0);
        state->phase++;
        break;
    case 1:
        if (world_thread_is_running(14))
            return 0;
        state->phase++;
        break;
    case 2:
        world_thread_start(14, world_text_message_box_thread);
        g_world_text_substitution_values[0] = g_wldcore_job_selection.speaker_party_index + TEXT_ID_UNIT_NAME_BASE;
        world_script_set_variable(
            0x5a, world_get_party_unit_formation_sprite(g_wldcore_job_selection.speaker_party_index));
        world_thread_set_parameters(14, 0x19,
            wldcore_proposition_adjust_message_index(message, g_wldcore_job_selection.speaker_party_index) + 0x8800, 0);
        state->phase++;
        break;
    case 3:
        if (!(g_wldcore_new_button_presses & PSX_PAD_CIRCLE))
            return 0;
        if (!world_menu_get_event_state_flag())
            return 0;
        state->phase++;
        break;
    case 4:
        world_thread_set_parameters(14, 0, -1, 0);
        state->phase++;
        break;
    case 5:
        if (world_thread_is_running(14))
            return 0;
        state->phase = 0;
        return 1;
    }
    return 0;
}
