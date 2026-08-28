#include "fft/main_runtime.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

/* Options-menu values: g_world_game_option_values holds the 13 options unpacked from
 * g_main_game_options plus an all-unchanged flag at [13]; g_world_game_option_original_values holds the
 * values they are compared against. */

/* Unpacks the configured options into the options-menu value table, then runs
 * main-menu mode 0 (options thread in the previous slot), 1 or 2. */
void world_menu_run_main_mode(s32 mode) {
    world_menu_entry_t* entry;
    u32 options;
    s32 i;
    s32 last_mode;

    entry = world_thread_get_current_parameter_1();
    options = g_main_game_options.value;
    g_world_game_option_values[GAME_OPTION_CURSOR_MOVEMENT] = options & GAME_OPTIONS_CURSOR_MOVEMENT_MASK;
    g_world_game_option_values[GAME_OPTION_CURSOR_REPEAT_SPEED]
        = (options >> GAME_OPTIONS_CURSOR_REPEAT_SPEED_SHIFT) & GAME_OPTIONS_THREE_BIT_VALUE_MASK;
    g_world_game_option_values[GAME_OPTION_MULTI_HEIGHT_CURSOR_SPEED]
        = (options >> GAME_OPTIONS_MULTI_HEIGHT_CURSOR_SPEED_SHIFT) & GAME_OPTIONS_THREE_BIT_VALUE_MASK;
    g_world_game_option_values[GAME_OPTION_FINGER_CURSOR_REPEAT_SPEED]
        = (options >> GAME_OPTIONS_FINGER_CURSOR_REPEAT_SPEED_SHIFT) & GAME_OPTIONS_THREE_BIT_VALUE_MASK;
    g_world_game_option_values[GAME_OPTION_MESSAGE_DISPLAY_SPEED]
        = (options >> GAME_OPTIONS_MESSAGE_DISPLAY_SPEED_SHIFT) & GAME_OPTIONS_THREE_BIT_VALUE_MASK;
    g_world_game_option_values[GAME_OPTION_NAVIGATION_MESSAGES]
        = (options >> GAME_OPTIONS_NAVIGATION_MESSAGES_SHIFT) & GAME_OPTIONS_TWO_BIT_VALUE_MASK;
    g_world_game_option_values[GAME_OPTION_ABILITY_NAMES]
        = (options >> GAME_OPTIONS_ABILITY_NAMES_SHIFT) & GAME_OPTIONS_TWO_BIT_VALUE_MASK;
    g_world_game_option_values[GAME_OPTION_EFFECT_MESSAGES]
        = (options >> GAME_OPTIONS_EFFECT_MESSAGES_SHIFT) & GAME_OPTIONS_TWO_BIT_VALUE_MASK;
    g_world_game_option_values[GAME_OPTION_DISPLAY_GAINED_EXP_JP]
        = (options >> GAME_OPTIONS_DISPLAY_GAINED_EXP_JP_SHIFT) & GAME_OPTIONS_TWO_BIT_VALUE_MASK;
    g_world_game_option_values[GAME_OPTION_TARGET_FLASHING]
        = (options >> GAME_OPTIONS_TARGET_FLASHING_SHIFT) & GAME_OPTIONS_TWO_BIT_VALUE_MASK;
    g_world_game_option_values[GAME_OPTION_SHOW_UNEQUIPPABLE_ITEMS]
        = (options >> GAME_OPTIONS_SHOW_UNEQUIPPABLE_ITEMS_SHIFT) & GAME_OPTIONS_TWO_BIT_VALUE_MASK;
    g_world_game_option_values[GAME_OPTION_MAX_EQUIP_AT_JOB_CHANGE]
        = (options >> GAME_OPTIONS_MAX_EQUIP_AT_JOB_CHANGE_SHIFT) & GAME_OPTIONS_TWO_BIT_VALUE_MASK;
    g_world_game_option_values[GAME_OPTION_SOUND_MODE]
        = (options >> GAME_OPTIONS_SOUND_MODE_SHIFT) & GAME_OPTIONS_TWO_BIT_VALUE_MASK;
    g_world_game_option_values[GAME_OPTION_UNCHANGED] = 1;
    for (i = 0; i < GAME_OPTION_COUNT; i++) {
        if (g_world_game_option_values[i] != g_world_game_option_original_values[i]) {
            g_world_game_option_values[GAME_OPTION_UNCHANGED] = 0;
        }
    }
    g_world_menu_transition_active = 0;
    g_world_menu_hide_numeric_values = 0;
    /* The target keeps this constant in a callee-saved register across the
     * mode 0 and mode 1 calls, so it is its own variable. */
    last_mode = 2;
    if (mode == 0) {
        world_script_pulse_tutorial_wait_value(0xFD);
        world_thread_start(g_world_thread_current_id - 1, world_build_options_menu);
        entry->select_text_table = 1;
        entry->text_binding = &g_world_option_menu_entry_table;
        world_thread_set_parameters(g_world_thread_current_id - 1, (s32)entry, 0, 0);
        world_thread_wait_until_inactive(g_world_thread_current_id - 1);
        world_thread_exit_current();
    }
    if (mode == 1) {
        world_script_pulse_tutorial_wait_value(0xF9);
        world_menu_start_at_list_thread();
    }
    if (mode == last_mode) {
        world_menu_start_dead_unit_threads();
    }
}
