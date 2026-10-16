/* The options overlay entry, loading the saved options and starting the game options, AT list or crystal pickup menus.
 */

#include "fft/battle_menu.h"
#include "fft/event_option.h"
#include "fft/main.h"
#include "fft/menu.h"
#include "fft/menu_at_list.h"
#include "fft/menu_crystal_pickup.h"
#include "fft/menu_input.h"
#include "fft/menu_option.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "psx/types.h"

/* OPTION.OUT entry, run on the battle menu thread whose first parameter is the menu
 * entry. Copy the saved options into g_menu_option_values and select the defaults row
 * (GAME_OPTION_UNCHANGED = 1) only when every value equals its reference value. Unless opening the crystal
 * pickup panel, ask the four battle info panels to stop and wait for them. Then run
 * `menu_type`'s screen: the game options on the thread slot below this one (waiting for
 * it, then exiting), the AT list, or the crystal pickup (crystal and treasure) panel.
 *
 * The values pulsed for tutorial scripts (0xfd options, 0xf9 AT list) are the same
 * screen codes WORLD's option entry pulses. */
void option_entrypoint(s32 menu_type) {
    menu_entry_t* menu = (menu_entry_t*)thread_get_current_parameter_1();
    s32 index;
    game_options_fields_t options = g_main_game_options.fields;

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

    for (index = 0; index < GAME_OPTION_COUNT; index++) {
        if (g_menu_option_values[index] != g_menu_option_reference_values[index]) {
            g_menu_option_values[GAME_OPTION_UNCHANGED] = 0;
        }
    }

    if (menu_type != MENU_OPTION_TYPE_CRYSTAL_PICKUP) {
        thread_set_parameters(THREAD_SLOT_TILE_INFO_PANEL, 0, 0, 1);
        thread_set_parameters(THREAD_SLOT_UNIT_SUMMARY_PANEL, 0, 0, 1);
        thread_set_parameters(THREAD_SLOT_RIGHT_UNIT_STATS, 0, 0, 1);
        thread_set_parameters(THREAD_SLOT_HOVERED_UNIT_STATS, 0, 0, 1);
        thread_wait_until_inactive(THREAD_SLOT_TILE_INFO_PANEL);
        thread_wait_until_inactive(THREAD_SLOT_UNIT_SUMMARY_PANEL);
        thread_wait_until_inactive(THREAD_SLOT_RIGHT_UNIT_STATS);
        thread_wait_until_inactive(THREAD_SLOT_HOVERED_UNIT_STATS);
    }

    g_menu_modal_open = 0;
    g_battle_option_menu_state = 0;
    if (menu_type == MENU_OPTION_TYPE_GAME_OPTIONS) {
        script_pulse_tutorial_wait_value(SCRIPT_TUTORIAL_WAIT_GAME_OPTIONS);
        thread_start(g_thread_current_id - 1, option_menu_run_game_options);
        menu->select_text_table = 1;
        menu->context = g_menu_option_entry_table;
        thread_set_parameters(g_thread_current_id - 1, menu, 0, 0);
        thread_wait_until_inactive(g_thread_current_id - 1);
        thread_exit_current();
    }
    if (menu_type == MENU_OPTION_TYPE_AT_LIST) {
        script_pulse_tutorial_wait_value(SCRIPT_TUTORIAL_WAIT_AT_LIST);
        menu_start_at_list_thread();
    }
    if (menu_type == MENU_OPTION_TYPE_CRYSTAL_PICKUP) {
        menu_start_crystal_pickup_threads();
    }
}
