#include "fft/main_sound.h"
#include "fft/require.h"
#include "fft/script_variables.h"
#include "fft/thread.h"
#include "psx/types.h"

void require_text_show_battle_congratulations(void) {
    s32 thread_a;
    s32 thread_b;

    g_battle_thread_call_target = main_sound_unload_scenario_music_and_tunes;
    battle_thread_call_on_main_stack();
    g_battle_thread_call_target = (void (*)(void))main_sound_open_scenario_smd_files;
    battle_thread_call_on_main_stack(battle_script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT) != 0x145 ? 0x2b : 0x2c);
    do {
        battle_thread_yield();
        g_battle_thread_call_target = (void (*)(void))main_sound_poll_scenario_smd_load;
    } while (battle_thread_call_on_main_stack() != 0);
    g_battle_thread_call_target = (void (*)(void))main_sound_switch_music_track;
    battle_thread_call_on_main_stack(1, 0x7f, 0);

    thread_a = battle_thread_resolve_id(0x10);
    battle_thread_start(thread_a, require_render_display_condition_special_cases_thread);
    battle_thread_set_parameters(thread_a, 1, 0, 0);
    if (battle_script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT) != 0x145)
        require_input_wait_frames_or_skip(0x1e);
    else
        battle_thread_wait_frames(0x1e);

    thread_b = battle_thread_resolve_id(0x10);
    battle_thread_start(thread_b, require_render_display_condition_special_cases_thread);
    battle_thread_set_parameters(thread_b, 7, 1, 0);
    if (battle_script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT) != 0x145)
        require_input_wait_frames_or_skip(0xb4);
    else
        battle_thread_wait_frames(0x12c);

    battle_thread_set_parameters(thread_a, 0, 0, 2);
    battle_thread_set_parameters(thread_b, 0, 0, 2);
    battle_thread_wait_until_inactive(thread_a);
    battle_thread_wait_until_inactive(thread_b);
    battle_thread_exit_current();
}
