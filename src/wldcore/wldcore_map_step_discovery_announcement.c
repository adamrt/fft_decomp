#include "fft/wldcore.h"
#include "psx/pad.h"

/* Phase driver for a countdown object: phase 1 waits for the menu event flag
 * and plays cue 0x214, phase 2 waits 180 frames and plays cue 0x221, phase 3
 * pops the menu level through 0x80077174 once both busy words clear; any
 * other phase steps the countdown on a confirm press (0x20). The busy words
 * are read as g_wldcore_audio_queue members, not scalars, which keeps the timer store
 * ahead of their loads as in the target. */
void wldcore_map_step_discovery_announcement(wldcore_menu_countdown_t* countdown) {
    if (countdown->phase == 3) {
        if (g_wldcore_audio_queue.count == 0 && g_wldcore_audio_queue.current_command == 0) {
            g_wldcore_menu_stack_depth--;
            wldcore_list_open_completed_propositions();
        }
    } else if (countdown->phase == 2) {
        countdown->timer++;
        if (g_wldcore_audio_queue.count == 0 && g_wldcore_audio_queue.current_command == 0 && countdown->timer >= 180) {
            countdown->phase = 3;
            wldcore_sound_enqueue_audio_command(2, 0x10);
            wldcore_sound_enqueue_audio_command(1, 0x221);
            wldcore_sound_enqueue_audio_command(3, 0x10);
        }
    } else if (countdown->phase == 1) {
        if (world_menu_get_event_state_flag() != 0 && g_wldcore_audio_queue.current_command == 0
            && g_wldcore_audio_queue.count == 0) {
            wldcore_sound_enqueue_audio_command(1, 0x214);
            wldcore_sound_enqueue_audio_command(3, 0x10);
            countdown->phase = 2;
        }
    } else if (g_wldcore_new_button_presses & PSX_PAD_CIRCLE) {
        wldcore_menu_show_next_sequence_message(countdown);
    }
}
