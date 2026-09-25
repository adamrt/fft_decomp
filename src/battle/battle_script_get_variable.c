#include "fft/battle.h"
#include "psx/types.h"

/* 0x8013b590: the word reached through 0x80165f9c is a temporary event
 * operand, not persistent map storage. Preserve it around interpreter calls.
 * Reading the combined weather/time variable refreshes its packed value first; GNS
 * mesh selection combines those upper bits with the layout's low 12 bits. */
s32 battle_script_get_variable(s32 variable_id) {
    s32 saved_current_variable = *g_battle_script_variables;

    if (variable_id == EVENT_SCRIPT_VAR_WEATHER_AND_TIME) {
        s32 time_of_day;
        s32 weather;

        time_of_day = battle_script_get_variable(EVENT_SCRIPT_VAR_TIME_OF_DAY);
        time_of_day &= 1;
        weather = battle_script_get_variable(EVENT_SCRIPT_VAR_WEATHER);
        weather &= 7;
        time_of_day <<= 15;
        weather <<= 12;
        battle_script_set_variable(EVENT_SCRIPT_VAR_WEATHER_AND_TIME, time_of_day | weather);
    }
    battle_script_run_variable_command(EVENT_OPCODE_ZERO_VARIABLE, 0, 0, 0);
    battle_script_run_variable_command(EVENT_OPCODE_ADD_VARIABLE, 0, variable_id, 0);
    {
        s32 value = *g_battle_script_variables;

        *g_battle_script_variables = saved_current_variable;
        return value;
    }
}
