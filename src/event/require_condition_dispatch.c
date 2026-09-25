#include "fft/event_require.h"
#include "psx/types.h"

void require_condition_dispatch(void) {
    s32 condition;
    s32 parameter;
    s32 thread;

    battle_thread_set_current_task_id(NATIVE_THREAD_TASK_CONDITION_DISPATCH);
    condition = battle_thread_get_current_parameter_1();
    parameter = battle_thread_get_current_parameter_2();
    if (condition >= 8)
        require_render_display_condition_general_cases(parameter);
    thread = battle_thread_resolve_id(0x10);

    if (condition == 0) {
        battle_thread_start(thread, require_render_display_condition_special_cases_thread);
        battle_thread_set_parameters(thread, 0, 0, 0);
        require_input_wait_frames_or_skip(parameter);
        battle_thread_set_parameters(thread, 0, 0, 2);
        battle_thread_wait_until_inactive(thread);
    } else if (condition == 1) {
        battle_thread_start(thread, require_condition_show_ready_bugged);
        battle_thread_set_parameters(thread, 0, 0, 0);
        battle_thread_wait_until_inactive(thread);
    } else if (condition == 2) {
        battle_thread_start(thread, require_text_show_battle_congratulations);
        battle_thread_set_parameters(thread, 0, 0, 0);
        battle_thread_wait_until_inactive(thread);
    } else if (condition == 3) {
        battle_thread_start(thread, require_render_gil_reward_ticker);
        battle_thread_set_parameters(thread, 0, 0, 0);
        battle_thread_wait_until_inactive(thread);
    } else if (condition == 4) {
        battle_thread_start(thread, require_render_war_trophies);
        battle_thread_set_parameters(thread, 0, 0, 0);
        battle_thread_wait_until_inactive(thread);
    } else if (condition == 5) {
        battle_thread_start(thread, require_party_apply_permanent_brave_faith_changes);
        battle_thread_set_parameters(thread, 0, 0, 0);
        battle_thread_wait_until_inactive(thread);
    } else if (condition == 6) {
        battle_thread_start(thread, require_party_remove_low_brave_high_faith_units);
        battle_thread_set_parameters(thread, 0, 0, 0);
        battle_thread_wait_until_inactive(thread);
    } else if (condition == 7) {
        require_apply_post_battle_unit_changes();
    }
    battle_thread_exit_current();
}
