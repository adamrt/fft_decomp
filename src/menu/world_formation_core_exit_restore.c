/* Restore after WLDCORE exit with thread result/state helpers. */

#include "fft/script.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

/*
 * Restore formation-menu resources and settings when WLDCORE requests exit.
 *
 * GPU work finishes before resources change. `EVENT_SCRIPT_VAR_FORMATION_RETURN_EVENT`
 * becomes `EVENT_SCRIPT_VAR_CURRENT_EVENT`, then the pending exit and auxiliary
 * flag are cleared.
 */
void world_formation_restore_menu_after_core_exit(void) {
    s32 profile;

    main_sound_unload_scenario_music_and_tunes();
    ClearImage(&g_world_screen_clear_rect, 0, 0, 0);
    DrawSync(0);
    VSync(0);
    world_menu_load_text_1bd8_with_face();
    DrawSync(0);
    VSync(0);
    world_formation_reset_menu_context();
    DrawSync(0);
    VSync(0);
    profile = g_main_game_options.fields.finger_cursor_repeat_speed;
    g_main_input_repeat_initial_delay = g_world_input_timing_profiles[profile].initial_delay;
    g_main_input_repeat_period = g_world_input_timing_profiles[profile].repeat_period;
    g_main_input_secondary_repeat_period = g_world_input_timing_profiles[profile].secondary_repeat_period;
    g_main_menu_scroll_accel_delay = g_world_input_timing_profiles[profile].scroll_accel_delay;
    g_main_menu_scroll_slow_step = g_world_input_timing_profiles[profile].scroll_slow_step;
    g_main_menu_scroll_fast_step = g_world_input_timing_profiles[profile].scroll_fast_step;
    world_menu_set_brightness(0x80, 0x80, 0x80);
    world_formation_run_screen(0, 0);
    world_formation_reset_menu_context();
    ClearImage(&g_world_screen_clear_rect, 0, 0, 0);
    DrawSync(0);
    script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, script_get_variable(EVENT_SCRIPT_VAR_FORMATION_RETURN_EVENT));
    script_set_variable(EVENT_SCRIPT_VAR_FORMATION_RETURN_EVENT, 0);
    if (script_get_variable(EVENT_SCRIPT_VAR_NEXT_SCENARIO) != 0) {
        script_set_variable(EVENT_SCRIPT_VAR_NEXT_SCENARIO, 0);
    }
}

s32 world_menu_take_and_clear_thread_result(void) {
    s32 result = g_world_menu_thread_result;

    g_world_menu_thread_result = 0;
    return result;
}

s32 world_menu_get_event_state_flag(void) {
    return g_world_menu_event_state_flag;
}

/* Ask a thread to stop by setting its task id to
 * THREAD_TASK_STOP_REQUEST. */
void world_thread_request_stop(s32 thread_id) {
    thread_t* thread = &g_threads[thread_id];

    thread->task_id = THREAD_TASK_STOP_REQUEST;
}
