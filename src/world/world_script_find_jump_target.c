#include "fft/event.h"

/* Resolves the script offset of a jump label.
 *
 * EVENT_OPCODE_BACK_TARGET scans forward from the start of the script and keeps
 * the last matching label before `offset`; any other instruction scans forward
 * from `offset` and takes the first match of either `instruction` or
 * `alt_instruction` (-1 disables the alternate). The returned offset points past
 * the opcode and its label byte. An unresolved label stops the current thread,
 * so the fall-through return value is never observed.
 *
 * The label fetches are spelled `*(base + index + 1)` rather than
 * `base[index + 1]`: the latter reassociates to `(index + base) + 1` and emits
 * the `addu` operands in the opposite order from the target.
 */
s32 world_script_find_jump_target(s32 offset, s32 instruction, s32 alt_instruction, s32 label) {
    s32 opcode;
    s32 i;
    s32 found;

    found = -1;
    if (instruction == EVENT_OPCODE_BACK_TARGET) {
        for (i = 0; i < offset; i += g_world_event_instruction_sizes[opcode] + 1) {
            opcode = g_world_event_script[i];
            if (opcode == EVENT_OPCODE_UNKNOWN_D9) {
                found = -1;
            } else if (opcode == EVENT_OPCODE_FORWARD_TARGET && *(g_world_event_script + i + 1) == label) {
                found = -1;
            } else if (opcode == instruction) {
                if (*(g_world_event_script + i + 1) == label) {
                    found = i;
                }
            }
        }
        if (found >= 0) {
            return found + 2;
        }
    } else {
        const u8* event;

        event = g_world_event_script;
        opcode = event[offset];
        while (opcode != EVENT_OPCODE_END) {
            if (opcode == instruction && *(event + offset + 1) == label) {
                return offset + 2;
            }
            if (alt_instruction != -1 && opcode == alt_instruction && *(g_world_event_script + offset + 1) == label) {
                return offset + 2;
            }
            offset += g_world_event_instruction_sizes[opcode] + 1;
            event = g_world_event_script;
            opcode = event[offset];
        }
    }
    world_thread_exit_current();
}
