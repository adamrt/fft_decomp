#include "fft/world.h"

/* Walks past the records that follow the header, returning the next one. */
menu_instruction_t* world_menu_skip_instruction_group(menu_instruction_t* instruction) {
    s32 count;

    if (instruction->header.opcode == MENU_SCRIPT_OP_IF_GROUP) {
        count = instruction->conditional.count;
        count += 1;
    } else if (instruction->header.opcode == MENU_SCRIPT_OP_IF_ELSE_GROUP) {
        count = instruction->if_else.count_true;
        count += instruction->if_else.count_false;
        count += 1;
    } else {
        count = 1;
    }
    count -= 1;
    while (count != -1) {
        instruction = menu_next_instruction(instruction);
        count -= 1;
    }
    return instruction;
}
