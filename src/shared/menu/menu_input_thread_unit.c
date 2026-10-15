#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"

/* Thread entry of system function 0x28 (the silent move confirmation): with menu
 * sounds suppressed, handle confirm and cancel on the menu each frame until
 * battle_menu_should_close_thread asks it to stop. The thread exits only when its
 * second parameter is 0. */
void menu_input_thread(void) {
    menu_entry_t* menu;

    menu = (menu_entry_t*)thread_get_current_parameter_1_variant();
    g_sound_suppressed = 1;
    while (1) {
        thread_yield();
        if (menu_should_close_thread_variant(&g_script_event_input) != 0) {
            break;
        }
        menu_handle_action(menu, 0);
        menu_handle_cancel_input(menu);
    }
    thread_yield();
    g_sound_suppressed = 0;
    if (thread_get_current_parameter_2_variant() == 0) {
        thread_exit_current();
    }
}

/* Read a script variable from any of the 32-bit variable, flag and nibble ranges by adding
 * it into 32-bit script variable 0 with the variable commands. Variable 0 (the comparison
 * result) is restored before returning.
 *
 * Reading the combined weather/time variable first rebuilds it: bit 15 from bit 0
 * of the time of day and bits 12-14 from the low three bits of the weather. GNS
 * mesh selection combines those upper bits with the layout's low 12 bits. */
s32 script_get_variable(s32 variable_id) {
    s32 saved_current_variable = *g_script_variables;

    if (variable_id == EVENT_SCRIPT_VAR_WEATHER_AND_TIME) {
        s32 time_of_day;
        s32 weather;

        time_of_day = script_get_variable(EVENT_SCRIPT_VAR_TIME_OF_DAY);
        time_of_day &= 1;
        weather = script_get_variable(EVENT_SCRIPT_VAR_WEATHER);
        weather &= 7;
        time_of_day <<= 15;
        weather <<= 12;
        script_set_variable(EVENT_SCRIPT_VAR_WEATHER_AND_TIME, time_of_day | weather);
    }
    script_run_variable_command(EVENT_OPCODE_ZERO_VARIABLE, EVENT_SCRIPT_VAR_COMPARISON_RESULT, 0, 0);
    script_run_variable_command(EVENT_OPCODE_ADD_VARIABLE, EVENT_SCRIPT_VAR_COMPARISON_RESULT, variable_id, 0);
    {
        s32 value = *g_script_variables;

        *g_script_variables = saved_current_variable;
        return value;
    }
}

/* Store a value in a script variable, whichever of the 32-bit variable, flag and nibble
 * ranges it belongs to, by running the ZeroVariable and AddImmediate commands on
 * it. 32-bit script variable 0 (the comparison result) is preserved around them, and war
 * funds are capped at 99,999,999. */
void script_set_variable(s32 variable_id, s32 value) {
    s32 saved_current_variable = *g_script_variables;

    if (variable_id == EVENT_SCRIPT_VAR_WAR_FUNDS && value > 0x05f5e0ff) {
        value = 0x05f5e0ff;
    }
    script_run_variable_command(EVENT_OPCODE_ZERO_VARIABLE, variable_id, 0, 0);
    script_run_variable_command(EVENT_OPCODE_ADD_IMMEDIATE, variable_id, value, 0);
    *g_script_variables = saved_current_variable;
}
