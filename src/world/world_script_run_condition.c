#include "fft/event.h"
#include "fft/world.h"
#include "psx/types.h"

void world_script_run_condition(event_opcode_e command) {
    if (command == EVENT_CONDITION_SECOND_GTE_FIRST) {
        s32* operands = g_world_script_variables;
        s32 operand_0 = operands[0];
        s32 operand_1 = operands[1];

        operands[0] = operand_1 >= operand_0;
    } else if (command == EVENT_CONDITION_FIRST_GTE_SECOND) {
        s32* operands = g_world_script_variables;
        s32 operand_0 = operands[0];
        s32 operand_1 = operands[1];

        operands[0] = operand_0 >= operand_1;
    } else if (command == EVENT_CONDITION_EQUAL) {
        s32* operands = g_world_script_variables;
        s32 operand_0 = operands[0];
        s32 operand_1 = operands[1];

        operands[0] = operand_0 == operand_1;
    } else if (command == EVENT_CONDITION_NOT_EQUAL) {
        s32* operands = g_world_script_variables;
        s32 operand_0 = operands[0];
        s32 operand_1 = operands[1];

        operands[0] = operand_0 != operand_1;
    } else if (command == EVENT_CONDITION_FIRST_LT_SECOND) {
        s32* operands = g_world_script_variables;
        s32 operand_0 = operands[0];
        s32 operand_1 = operands[1];

        operands[0] = operand_0 < operand_1;
    } else if (command == EVENT_CONDITION_SECOND_LT_FIRST) {
        s32* operands = g_world_script_variables;
        s32 operand_0 = operands[0];
        s32 operand_1 = operands[1];

        operands[0] = operand_1 < operand_0;
    }
}
