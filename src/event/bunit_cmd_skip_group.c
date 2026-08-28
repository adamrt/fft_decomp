#include "psx/types.h"

/* Skip a command plus, for group opcodes 1 and 2, the child commands it counts;
 * return the command that follows. */
u8* bunit_cmd_skip_group(u8* command) {
    s32 count;
    s32 opcode = command[0];
    if (opcode == 1) {
        count = command[4] + 1;
    } else if (opcode == 2) {
        count = command[4] + command[5] + 1;
    } else {
        count = 1;
    }
    count -= 1;
    if (count != -1) {
        do {
            count -= 1;
            command += command[1];
        } while (count != -1);
    }
    return command;
}
