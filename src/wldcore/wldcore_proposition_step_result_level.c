#include "fft/data.h"
#include "fft/main_runtime.h"
#include "fft/main_sound.h"
#include "fft/script_variables.h"
#include "fft/thread.h"
#include "fft/wldcore.h"
#include "fft/world.h"
#include "psx/pad.h"

s32 main_job_add_proposition_jp(s32 party_index, s32 jp);
void wldcore_list_open_completed_propositions(void);
s32 wldcore_get_completion_milestone_rank(void);
void wldcore_menu_push_countdown_level(void);
void wldcore_write_nine_bit_record(u32* bits, s32 record, u8* source);

/* Per-frame step of the proposition-result menu level.
 *
 * Phase 0 waits for confirm/cancel and then commits the result: it clears the
 * proposition's script flag bit 1 (and sets bit 4 on success), adds the reward
 * total g_wldcore_proposition_gil_amount to gil (saturating at 99,999,999), records the completion
 * date in the packed nine-bit arrays, releases each participant from the
 * proposition and pays them their share, marks the proposition
 * state byte, and finally compacts the participant record out of
 * g_main_active_propositions. Phase 1 waits for WORLD thread 14 and the audio
 * queue to drain and restarts the town music; phase 2 pops the level and hands
 * off to the countdown level or to wldcore_list_open_completed_propositions.
 *
 * The proposition, result and share words are read as g_wldcore_job_selection
 * members: the target keeps that block's address in a register, which GCC
 * only does for an aggregate view, not for the scalar g_wldcore_job_selection.proposition_index names. */
void wldcore_proposition_step_result_level(wldcore_proposition_result_level_t* level) {
    u8 date[2];
    s32 proposition_flags;
    s32 war_funds;
    s32 day;
    s32 month;
    s32 i;

    if (g_wldcore_window_render_records[level->render_index].flags & 0x100) {
        return;
    }
    if (level->phase == 2) {
        if (g_wldcore_audio_queue.count == 0 && g_wldcore_audio_queue.current_command == 0) {
            g_wldcore_menu_stack_depth = g_wldcore_menu_stack_depth - 1;
            if (wldcore_get_completion_milestone_rank() != 0) {
                wldcore_menu_push_countdown_level();
                return;
            }
            if (g_wldcore_job_selection.gate == 0) {
                wldcore_sound_enqueue_audio_command(3, 0x10);
            }
            wldcore_list_open_completed_propositions();
        }
        return;
    }
    if (level->phase == 1) {
        if (g_wldcore_audio_queue.count == 0 && g_wldcore_audio_queue.current_command == 0
            && world_thread_is_running(0xE) == 0) {
            if (wldcore_get_completion_milestone_rank() == 0 && g_wldcore_job_selection.gate == 0) {
                wldcore_sound_enqueue_audio_command(1, 0x221);
            }
            level->phase = 2;
        }
        return;
    }

    if (level->delay != 0) {
        level->delay = level->delay - 1;
        if (level->delay == 0) {
            main_sound_play_2_sfx(0x4D, 0x4E);
        }
    }
    if (!(g_wldcore_new_button_presses & (PSX_PAD_CIRCLE | PSX_PAD_CROSS))) {
        return;
    }
    g_wldcore_window_render_record_count = g_wldcore_window_render_record_count - 1;
    g_wldcore_window_render_object_count = g_wldcore_window_render_object_count - 1;
    proposition_flags
        = world_script_get_variable(g_main_active_propositions[level->proposition].proposition_id + 0x360) & 0xFFFD;
    if (g_wldcore_job_selection.result == 0) {
        proposition_flags |= 4;
    }
    world_script_set_variable(g_main_active_propositions[level->proposition].proposition_id + 0x360, proposition_flags);
    war_funds = world_script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS) + g_wldcore_proposition_gil_amount;
    if (war_funds > 0x05F5E0FF) {
        war_funds = 0x05F5E0FF;
    }
    world_script_set_variable(EVENT_SCRIPT_VAR_WAR_FUNDS, war_funds);
    g_wldcore_displayed_numeric_value = world_script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS);
    day = world_script_get_variable(EVENT_SCRIPT_VAR_DAY);
    month = world_script_get_variable(EVENT_SCRIPT_VAR_MONTH);
    if (g_wldcore_job_selection.result == 0) {
        date[1] = day;
        date[0] = month;
        wldcore_write_nine_bit_record(g_main_proposition_last_attempt_date_bits,
            g_main_active_propositions[level->proposition].proposition_id, date);
    }
    if (g_wldcore_job_selection.reward_type == 1) {
        world_script_set_variable(g_wldcore_job_selection.reward_index + 0x321, 1);
        date[1] = day;
        date[0] = month;
        wldcore_write_nine_bit_record(
            g_main_treasure_acquisition_date_bits, g_wldcore_job_selection.reward_index, date);
    }
    if (g_wldcore_job_selection.reward_type == 2) {
        world_script_set_variable(g_wldcore_job_selection.reward_index + 0x350, 1);
        date[1] = day;
        date[0] = month;
        wldcore_write_nine_bit_record(g_main_land_discovery_date_bits, g_wldcore_job_selection.reward_index, date);
    }
    for (i = 0; i < g_main_active_propositions[g_wldcore_job_selection.proposition_index].participant_count; i++) {
        wldcore_get_party_data_pointer(
            g_main_active_propositions[g_wldcore_job_selection.proposition_index].participant_indices[i])
            ->proposition_status = 0;
        main_job_add_proposition_jp(
            g_main_active_propositions[g_wldcore_job_selection.proposition_index].participant_indices[i],
            g_wldcore_job_selection.rows[0][i]);
    }
    if (g_wldcore_job_selection.result == 1) {
        g_main_proposition_states[g_main_active_propositions[g_wldcore_job_selection.proposition_index].proposition_id]
            = 0x60;
    }
    if (g_wldcore_job_selection.result == 2) {
        g_main_proposition_states[g_main_active_propositions[g_wldcore_job_selection.proposition_index].proposition_id]
            = 0x90;
    }
    for (i = level->proposition; i < g_main_save_proposition_count - 1; i++) {
        g_main_active_propositions[i] = g_main_active_propositions[i + 1];
    }
    g_main_save_proposition_count--;
    if (wldcore_get_completion_milestone_rank() == 0 && g_wldcore_job_selection.gate == 0) {
        wldcore_sound_enqueue_audio_command(2, 0x10);
    }
    world_thread_set_parameters(0xE, 0, -1, 0);
    level->phase = 1;
}
