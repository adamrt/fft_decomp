#include "fft/event.h"
#include "fft/script_variables.h"
#include "fft/world.h"
#include "psx/types.h"

s32 world_script_get_variable(s32 variable_id) {
    s32 saved = *g_world_script_variables;

    if (variable_id == EVENT_SCRIPT_VAR_WEATHER_AND_TIME) {
        s32 tod;
        s32 wth;

        tod = world_script_get_variable(EVENT_SCRIPT_VAR_TIME_OF_DAY);
        tod &= 1;
        wth = world_script_get_variable(EVENT_SCRIPT_VAR_WEATHER);
        wth &= 7;
        tod <<= 15;
        wth <<= 12;
        world_script_set_variable(EVENT_SCRIPT_VAR_WEATHER_AND_TIME, tod | wth);
    }
    world_script_run_variable_command(EVENT_OPCODE_ZERO_VARIABLE, 0, 0, 0);
    world_script_run_variable_command(EVENT_OPCODE_ADD_VARIABLE, 0, variable_id, 0);
    {
        s32 val = *g_world_script_variables;

        *g_world_script_variables = saved;
        return val;
    }
}
