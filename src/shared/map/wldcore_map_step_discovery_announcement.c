#include "fft/wldcore.h"
#include "psx/pad.h"

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
