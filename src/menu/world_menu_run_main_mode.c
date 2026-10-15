#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

/* Options-menu values: g_world_game_option_values holds the 13 options unpacked from
 * g_main_game_options plus an all-unchanged flag at [13]; g_world_game_option_original_values holds the
 * values they are compared against. */

/* Unpacks the configured options into the options-menu value table, then runs
 * main-menu mode 0 (options thread in the previous slot), 1 or 2. */
void world_menu_run_main_mode(s32 mode) {
    menu_entry_t* entry;
    game_options_fields_t options;
    s32 i;
    s32 last_mode;

    entry = (menu_entry_t*)thread_get_current_parameter_1_variant();
    options = g_main_game_options.fields;
    g_menu_option_values[GAME_OPTION_CURSOR_MOVEMENT] = options.cursor_movement;
    g_menu_option_values[GAME_OPTION_CURSOR_REPEAT_SPEED] = options.cursor_repeat_speed;
    g_menu_option_values[GAME_OPTION_MULTI_HEIGHT_CURSOR_SPEED] = options.multi_height_cursor_speed;
    g_menu_option_values[GAME_OPTION_FINGER_CURSOR_REPEAT_SPEED] = options.finger_cursor_repeat_speed;
    g_menu_option_values[GAME_OPTION_MESSAGE_DISPLAY_SPEED] = options.message_display_speed;
    g_menu_option_values[GAME_OPTION_NAVIGATION_MESSAGES] = options.navigation_messages;
    g_menu_option_values[GAME_OPTION_ABILITY_NAMES] = options.ability_names;
    g_menu_option_values[GAME_OPTION_EFFECT_MESSAGES] = options.effect_messages;
    g_menu_option_values[GAME_OPTION_DISPLAY_GAINED_EXP_JP] = options.display_gained_exp_jp;
    g_menu_option_values[GAME_OPTION_TARGET_FLASHING] = options.target_flashing;
    g_menu_option_values[GAME_OPTION_SHOW_UNEQUIPPABLE_ITEMS] = options.show_unequippable_items;
    g_menu_option_values[GAME_OPTION_MAX_EQUIP_AT_JOB_CHANGE] = options.max_equip_at_job_change;
    g_menu_option_values[GAME_OPTION_SOUND_MODE] = options.sound_mode;
    g_menu_option_values[GAME_OPTION_UNCHANGED] = 1;
    for (i = 0; i < GAME_OPTION_COUNT; i++) {
        if (g_menu_option_values[i] != g_menu_option_reference_values[i]) {
            g_menu_option_values[GAME_OPTION_UNCHANGED] = 0;
        }
    }
    g_menu_modal_open = 0;
    g_menu_hide_numeric_values = 0;
    /* The target keeps this constant in a callee-saved register across the
     * mode 0 and mode 1 calls, so it is its own variable. */
    last_mode = OPTION_MENU_DEAD_UNIT;
    if (mode == OPTION_MENU_GAME_OPTIONS) {
        script_pulse_tutorial_wait_value(0xFD);
        thread_start(g_thread_current_id - 1, menu_run_game_options_variant);
        entry->select_text_table = 1;
        entry->context = &g_menu_option_entry_table;
        thread_set_parameters(g_thread_current_id - 1, (s32)entry, 0, 0);
        thread_wait_until_inactive(g_thread_current_id - 1);
        thread_exit_current();
    }
    if (mode == OPTION_MENU_AT_LIST) {
        script_pulse_tutorial_wait_value(0xF9);
        menu_start_at_list_thread();
    }
    if (mode == last_mode) {
        menu_start_dead_unit_threads();
    }
}
