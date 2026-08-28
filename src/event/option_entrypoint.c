#include "fft/battle_runtime.h"
#include "fft/main_runtime.h"
#include "fft/option.h"
#include "psx/types.h"

/* Decode saved option fields and dispatch the requested option-menu mode. */
void option_entrypoint(s32 menu_type) {
    u8* menu = (u8*)battle_thread_get_current_parameter_1();
    s32 index;
    u32 packed = g_main_game_options.value;

    g_option_values_unchanged = 1;
    g_option_current_values[GAME_OPTION_CURSOR_MOVEMENT] = packed & GAME_OPTIONS_CURSOR_MOVEMENT_MASK;
    g_option_current_values[GAME_OPTION_CURSOR_REPEAT_SPEED]
        = (packed >> GAME_OPTIONS_CURSOR_REPEAT_SPEED_SHIFT) & GAME_OPTIONS_THREE_BIT_VALUE_MASK;
    g_option_current_values[GAME_OPTION_MULTI_HEIGHT_CURSOR_SPEED]
        = (packed >> GAME_OPTIONS_MULTI_HEIGHT_CURSOR_SPEED_SHIFT) & GAME_OPTIONS_THREE_BIT_VALUE_MASK;
    g_option_current_values[GAME_OPTION_FINGER_CURSOR_REPEAT_SPEED]
        = (packed >> GAME_OPTIONS_FINGER_CURSOR_REPEAT_SPEED_SHIFT) & GAME_OPTIONS_THREE_BIT_VALUE_MASK;
    g_option_current_values[GAME_OPTION_MESSAGE_DISPLAY_SPEED]
        = (packed >> GAME_OPTIONS_MESSAGE_DISPLAY_SPEED_SHIFT) & GAME_OPTIONS_THREE_BIT_VALUE_MASK;
    g_option_current_values[GAME_OPTION_NAVIGATION_MESSAGES]
        = (packed >> GAME_OPTIONS_NAVIGATION_MESSAGES_SHIFT) & GAME_OPTIONS_TWO_BIT_VALUE_MASK;
    g_option_current_values[GAME_OPTION_ABILITY_NAMES]
        = (packed >> GAME_OPTIONS_ABILITY_NAMES_SHIFT) & GAME_OPTIONS_TWO_BIT_VALUE_MASK;
    g_option_current_values[GAME_OPTION_EFFECT_MESSAGES]
        = (packed >> GAME_OPTIONS_EFFECT_MESSAGES_SHIFT) & GAME_OPTIONS_TWO_BIT_VALUE_MASK;
    g_option_current_values[GAME_OPTION_DISPLAY_GAINED_EXP_JP]
        = (packed >> GAME_OPTIONS_DISPLAY_GAINED_EXP_JP_SHIFT) & GAME_OPTIONS_TWO_BIT_VALUE_MASK;
    g_option_current_values[GAME_OPTION_TARGET_FLASHING]
        = (packed >> GAME_OPTIONS_TARGET_FLASHING_SHIFT) & GAME_OPTIONS_TWO_BIT_VALUE_MASK;
    g_option_current_values[GAME_OPTION_SHOW_UNEQUIPPABLE_ITEMS]
        = (packed >> GAME_OPTIONS_SHOW_UNEQUIPPABLE_ITEMS_SHIFT) & GAME_OPTIONS_TWO_BIT_VALUE_MASK;
    g_option_current_values[GAME_OPTION_MAX_EQUIP_AT_JOB_CHANGE]
        = (packed >> GAME_OPTIONS_MAX_EQUIP_AT_JOB_CHANGE_SHIFT) & GAME_OPTIONS_TWO_BIT_VALUE_MASK;
    g_option_current_values[GAME_OPTION_SOUND_MODE]
        = (packed >> GAME_OPTIONS_SOUND_MODE_SHIFT) & GAME_OPTIONS_TWO_BIT_VALUE_MASK;

    for (index = 0; index < GAME_OPTION_COUNT; index++) {
        if (g_option_current_values[index] != g_option_reference_values[index]) {
            g_option_current_values[GAME_OPTION_UNCHANGED] = 0;
        }
    }

    if (menu_type != 2) {
        battle_thread_set_parameters(10, 0, 0, 1);
        battle_thread_set_parameters(11, 0, 0, 1);
        battle_thread_set_parameters(12, 0, 0, 1);
        battle_thread_set_parameters(13, 0, 0, 1);
        battle_thread_wait_until_inactive(10);
        battle_thread_wait_until_inactive(11);
        battle_thread_wait_until_inactive(12);
        battle_thread_wait_until_inactive(13);
    }

    g_option_menu_open = 0;
    g_option_menu_state = 0;
    if (menu_type == 0) {
        s32 child_thread;

        battle_script_pulse_tutorial_wait_value(0xfd);
        battle_thread_start(g_battle_current_thread_id - 1, option_build_options_menu);
        child_thread = g_battle_current_thread_id;
        *(s16*)(menu + 0x3a) = 1;
        *(void**)(menu + 0x30) = g_option_menu_entry_table;
        battle_thread_set_parameters(child_thread - 1, menu, 0, 0);
        battle_thread_wait_until_inactive(g_battle_current_thread_id - 1);
        battle_thread_exit_current();
    }
    if (menu_type == 1) {
        battle_script_pulse_tutorial_wait_value(0xf9);
        option_menu_start_at_list_thread();
    }
    if (menu_type == 2) {
        option_menu_start_dead_unit_threads();
    }
}
