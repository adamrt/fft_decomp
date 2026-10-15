/* Proposition report countdown and milestone announcement. */

#include "fft/script.h"
#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/pad.h"

/* Pushes a countdown menu level (type 0x37): the first message is 0xb8f9
 * when g_wldcore_proposition_report.reward_type is 1, otherwise 0xb904; a count of 1 from 0x8007a72c runs
 * four messages, any other count runs one message starting count + 2 further
 * on. Starts thread 14 if idle and shows the first message. */
void wldcore_menu_push_countdown_level(void) {
    s32 count;

    count = wldcore_get_completion_milestone_rank();
    if (g_wldcore_proposition_report.reward_type == WLDCORE_PROPOSITION_REWARD_TREASURE) {
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].countdown.current
            = WLDCORE_TEXT_ID_TREASURE_FOUND_FIRST;
    } else {
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].countdown.current
            = WLDCORE_TEXT_ID_LAND_FOUND_FIRST;
    }
    if (count == 1) {
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].countdown.remaining = 4;
    } else {
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].countdown.remaining = 1;
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].countdown.current += 2 + count;
    }
    if (thread_is_running(WLDCORE_THREAD_SLOT_MESSAGE) == 0) {
        thread_start(WLDCORE_THREAD_SLOT_MESSAGE, world_text_message_box_thread);
    }
    wldcore_menu_show_next_sequence_message(&g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].countdown);
    g_wldcore_menu_stack_types[g_wldcore_menu_stack_depth + 1] = WLDCORE_MENU_LEVEL_COUNTDOWN;
    g_wldcore_menu_stack_depth++;
}

/* Show the countdown's next message and count it off; the last one also
 * fades the music out and moves the countdown to
 * WLDCORE_COUNTDOWN_PHASE_LAST_MESSAGE with the timer cleared. */
void wldcore_menu_show_next_sequence_message(wldcore_menu_countdown_t* countdown) {
    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, countdown->current, 0);
    countdown->current++;
    countdown->remaining--;
    if (countdown->remaining == 0) {
        wldcore_sound_enqueue_audio_command(WLDCORE_AUDIO_COMMAND_FADE_OUT, 4);
        countdown->phase = WLDCORE_COUNTDOWN_PHASE_LAST_MESSAGE;
        countdown->timer = 0;
    } else {
        countdown->phase = WLDCORE_COUNTDOWN_PHASE_MESSAGES;
    }
}

/* Return the one-based treasure or unexplored-land collection milestone
 * reached by a successful proposition, or zero when the count is not a milestone. */
s32 wldcore_get_completion_milestone_rank(void) {
    s32 count;
    s32 i;
    s32 index;

    if (g_wldcore_proposition_report.result != WLDCORE_PROPOSITION_RESULT_SUCCESS) {
        return 0;
    }
    if (g_wldcore_proposition_report.reward_type != WLDCORE_PROPOSITION_REWARD_TREASURE
        && g_wldcore_proposition_report.reward_type != WLDCORE_PROPOSITION_REWARD_UNEXPLORED_LAND) {
        return 0;
    }
    if (g_wldcore_proposition_report.reward_type == WLDCORE_PROPOSITION_REWARD_TREASURE) {
        count = 0;
        for (i = 0; i < 31; i++) {
            if (script_get_variable(i + EVENT_SCRIPT_VAR_TREASURE_FIRST) != 0) {
                count++;
            }
        }
        for (i = 0; i < 8; i++) {
            if (g_wldcore_treasure_count_milestones[i] == count) {
                break;
            }
        }
        if (i == 8) {
            return 0;
        }
        index = i;
    }
    if (g_wldcore_proposition_report.reward_type == WLDCORE_PROPOSITION_REWARD_UNEXPLORED_LAND) {
        count = 0;
        for (i = 0; i < EVENT_SCRIPT_VAR_UNEXPLORED_LAND_COUNT; i++) {
            if (script_get_variable(i + EVENT_SCRIPT_VAR_UNEXPLORED_LAND_FIRST) != 0) {
                count++;
            }
        }
        for (i = 0; i < 8; i++) {
            if (g_wldcore_unexplored_land_count_milestones[i] == count) {
                break;
            }
        }
        if (i == 8) {
            return 0;
        }
        index = i;
    }
    return index + 1;
}

/* Phase driver for a countdown object: phase 1 waits for the menu event flag
 * and plays the milestone music, phase 2 waits 180 frames and returns to the bar music, phase 3
 * pops the menu level through 0x80077174 once both 32-bit busy fields clear; any
 * other phase steps the countdown on a confirm press (0x20). The 32-bit busy fields
 * are read as g_wldcore_audio_queue members, not scalars, which keeps the timer store
 * ahead of their loads as in the target. */
void wldcore_map_step_discovery_announcement(wldcore_menu_countdown_t* countdown) {
    if (countdown->phase == WLDCORE_COUNTDOWN_PHASE_CLOSE) {
        if (g_wldcore_audio_queue.count == 0 && g_wldcore_audio_queue.current_command == WLDCORE_AUDIO_COMMAND_NONE) {
            g_wldcore_menu_stack_depth--;
            wldcore_list_open_completed_propositions();
        }
    } else if (countdown->phase == WLDCORE_COUNTDOWN_PHASE_JINGLE) {
        countdown->timer++;
        if (g_wldcore_audio_queue.count == 0 && g_wldcore_audio_queue.current_command == WLDCORE_AUDIO_COMMAND_NONE
            && countdown->timer >= 180) {
            countdown->phase = WLDCORE_COUNTDOWN_PHASE_CLOSE;
            wldcore_sound_enqueue_audio_command(WLDCORE_AUDIO_COMMAND_FADE_OUT, 0x10);
            wldcore_sound_enqueue_audio_command(
                WLDCORE_AUDIO_COMMAND_PLAY_MUSIC, WLDCORE_AUDIO_MUSIC(2, MAIN_MUSIC_BAR));
            wldcore_sound_enqueue_audio_command(WLDCORE_AUDIO_COMMAND_FADE_IN, 0x10);
        }
    } else if (countdown->phase == WLDCORE_COUNTDOWN_PHASE_LAST_MESSAGE) {
        if (world_menu_get_event_state_flag() != 0
            && g_wldcore_audio_queue.current_command == WLDCORE_AUDIO_COMMAND_NONE
            && g_wldcore_audio_queue.count == 0) {
            wldcore_sound_enqueue_audio_command(
                WLDCORE_AUDIO_COMMAND_PLAY_MUSIC, WLDCORE_AUDIO_MUSIC(2, MAIN_MUSIC_MILESTONE));
            wldcore_sound_enqueue_audio_command(WLDCORE_AUDIO_COMMAND_FADE_IN, 0x10);
            countdown->phase = WLDCORE_COUNTDOWN_PHASE_JINGLE;
        }
    } else if (g_wldcore_input_new_button_presses & PSX_PAD_CIRCLE) {
        wldcore_menu_show_next_sequence_message(countdown);
    }
}
