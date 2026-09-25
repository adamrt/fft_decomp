#include "fft/main_runtime.h"
#include "fft/main_sound.h"
#include "fft/script_variables.h"
#include "fft/world.h"

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
    world_script_set_variable(
        EVENT_SCRIPT_VAR_CURRENT_EVENT, world_script_get_variable(EVENT_SCRIPT_VAR_FORMATION_RETURN_EVENT));
    world_script_set_variable(EVENT_SCRIPT_VAR_FORMATION_RETURN_EVENT, 0);
    if (world_script_get_variable(EVENT_SCRIPT_VAR_NEXT_SCENARIO) != 0) {
        world_script_set_variable(EVENT_SCRIPT_VAR_NEXT_SCENARIO, 0);
    }
}
