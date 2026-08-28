#include "fft/battle.h"
#include "fft/event.h"

void battle_script_run_condition(event_opcode_e opcode) {
    if (opcode == EVENT_CONDITION_SECOND_GTE_FIRST) {
        g_battle_script_variables[0] = g_battle_script_variables[0] <= g_battle_script_variables[1];
        return;
    }
    if (opcode == EVENT_CONDITION_FIRST_GTE_SECOND) {
        g_battle_script_variables[0] = g_battle_script_variables[0] >= g_battle_script_variables[1];
        return;
    }
    if (opcode == EVENT_CONDITION_EQUAL) {
        g_battle_script_variables[0] = g_battle_script_variables[0] == g_battle_script_variables[1];
        return;
    }
    if (opcode == EVENT_CONDITION_NOT_EQUAL) {
        g_battle_script_variables[0] = g_battle_script_variables[0] != g_battle_script_variables[1];
        return;
    }
    if (opcode == EVENT_CONDITION_FIRST_LT_SECOND) {
        g_battle_script_variables[0] = g_battle_script_variables[0] < g_battle_script_variables[1];
        return;
    }
    if (opcode == EVENT_CONDITION_SECOND_LT_FIRST) {
        g_battle_script_variables[0] = g_battle_script_variables[0] > g_battle_script_variables[1];
        return;
    }
}
