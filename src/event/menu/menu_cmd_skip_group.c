#include "fft/menu_script.h"

/* Skip a command plus, for group opcodes 1 and 2, the child commands it counts;
 * return the command that follows. */
menu_instruction_t* menu_cmd_skip_group(menu_instruction_t* command) {
    s32 count;

    if (command->header.opcode == MENU_SCRIPT_OP_IF_GROUP) {
        count = command->conditional.count + 1;
    } else if (command->header.opcode == MENU_SCRIPT_OP_IF_ELSE_GROUP) {
        count = command->if_else.count_true + command->if_else.count_false + 1;
    } else {
        count = 1;
    }

    while (--count != -1) {
        command = menu_next_instruction(command);
    }
    return command;
}
