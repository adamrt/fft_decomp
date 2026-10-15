#include "fft/battle_script.h"
#include "fft/event_require.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "psx/types.h"

/*
 * Thread body of the "Congratulations!" battle-complete display.
 *
 * Swaps the scenario music for the victory track, shows banner 1 and, half a
 * second later, banner 7, holds them, then fades both out and waits for
 * their threads. During event 0x145 a different track plays, the waits
 * cannot be skipped and the hold is 300 frames in place of 180.
 */
void require_text_show_battle_congratulations(void) {
    s32 thread_a;
    s32 thread_b;

    g_thread_call_target = main_sound_unload_scenario_music_and_tunes;
    thread_call_on_main_stack();
    g_thread_call_target = (void (*)(void))main_sound_open_scenario_smd_files;
    thread_call_on_main_stack(script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT) != EVENT_ID_FINAL_BATTLE_WON
            ? MAIN_MUSIC_CONGRATULATIONS
            : MAIN_MUSIC_CONGRATULATIONS_FINAL_BATTLE);
    do {
        thread_yield();
        g_thread_call_target = (void (*)(void))main_sound_poll_scenario_smd_load;
    } while (thread_call_on_main_stack() != 0);
    g_thread_call_target = (void (*)(void))main_sound_switch_music_track;
    thread_call_on_main_stack(1, 0x7f, 0);

    thread_a = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
    thread_start(thread_a, require_condition_run_special_thread);
    thread_set_parameters(thread_a, 1, 0, 0);
    if (script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT) != EVENT_ID_FINAL_BATTLE_WON)
        require_input_wait_frames_or_skip(0x1e);
    else
        thread_wait_frames(0x1e);

    thread_b = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
    thread_start(thread_b, require_condition_run_special_thread);
    thread_set_parameters(thread_b, 7, 1, 0);
    if (script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT) != EVENT_ID_FINAL_BATTLE_WON)
        require_input_wait_frames_or_skip(0xb4);
    else
        thread_wait_frames(0x12c);

    thread_set_parameters(thread_a, 0, 0, 2);
    thread_set_parameters(thread_b, 0, 0, 2);
    thread_wait_until_inactive(thread_a);
    thread_wait_until_inactive(thread_b);
    thread_exit_current();
}
