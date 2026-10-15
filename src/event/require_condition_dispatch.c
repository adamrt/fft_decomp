#include "fft/event_require.h"
#include "fft/thread.h"
#include "psx/types.h"

/*
 * Thread body of the REQUIRE overlay: runs one display condition.
 *
 * Parameter 1 is the condition id and parameter 2 its argument. The general
 * conditions draw their banner inline (the argument is the hold time). The
 * others each run their routine in the first idle thread slot and wait for
 * it: the special-case banner (held for the argument's frames or until
 * skipped), "ready", the battle congratulations, the gil reward, the war
 * trophies, the permanent Brave/Faith changes and the low-Brave/high-Faith
 * desertions. The post-battle unit changes run inline.
 */
void require_condition_dispatch(void) {
    s32 condition;
    s32 parameter;
    s32 thread;

    thread_set_current_task_id(THREAD_TASK_CONDITION_DISPATCH);
    condition = thread_get_current_parameter_1_variant();
    parameter = thread_get_current_parameter_2_variant();
    if (condition >= EVENT_DISPLAY_CONDITION_GENERAL_FIRST)
        require_condition_show_general(parameter);
    thread = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);

    if (condition == EVENT_DISPLAY_CONDITION_SPECIAL_CASES) {
        thread_start(thread, require_condition_run_special_thread);
        thread_set_parameters(thread, 0, 0, 0);
        require_input_wait_frames_or_skip(parameter);
        thread_set_parameters(thread, 0, 0, 2);
        thread_wait_until_inactive(thread);
    } else if (condition == EVENT_DISPLAY_CONDITION_SHOW_READY_BUGGED) {
        thread_start(thread, require_condition_show_ready_bugged);
        thread_set_parameters(thread, 0, 0, 0);
        thread_wait_until_inactive(thread);
    } else if (condition == EVENT_DISPLAY_CONDITION_BATTLE_CONGRATULATIONS) {
        thread_start(thread, require_text_show_battle_congratulations);
        thread_set_parameters(thread, 0, 0, 0);
        thread_wait_until_inactive(thread);
    } else if (condition == EVENT_DISPLAY_CONDITION_GIL_REWARD) {
        thread_start(thread, require_reward_run_gil_ticker);
        thread_set_parameters(thread, 0, 0, 0);
        thread_wait_until_inactive(thread);
    } else if (condition == EVENT_DISPLAY_CONDITION_WAR_TROPHIES) {
        thread_start(thread, require_reward_run_war_trophies);
        thread_set_parameters(thread, 0, 0, 0);
        thread_wait_until_inactive(thread);
    } else if (condition == EVENT_DISPLAY_CONDITION_APPLY_PERMANENT_BRAVE_FAITH_CHANGES) {
        thread_start(thread, require_party_report_brave_faith_changes);
        thread_set_parameters(thread, 0, 0, 0);
        thread_wait_until_inactive(thread);
    } else if (condition == EVENT_DISPLAY_CONDITION_REMOVE_LOW_BRAVE_HIGH_FAITH_UNITS) {
        thread_start(thread, require_party_remove_low_brave_high_faith_units);
        thread_set_parameters(thread, 0, 0, 0);
        thread_wait_until_inactive(thread);
    } else if (condition == EVENT_DISPLAY_CONDITION_APPLY_POST_BATTLE_UNIT_CHANGES) {
        require_party_apply_post_battle_changes();
    }
    thread_exit_current();
}
