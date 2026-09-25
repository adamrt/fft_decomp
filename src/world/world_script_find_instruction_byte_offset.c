#include "fft/world.h"

s32 world_script_find_instruction_byte_offset(s32 offset, s32 instruction) {
    const u8* event = g_world_event_script;

    do {
        u8 opcode = event[offset];

        if (opcode == EVENT_OPCODE_END) {
            return 0;
        }
        if (opcode == instruction) {
            return offset;
        }
        offset += g_world_event_instruction_sizes[opcode] + 1;
    } while (1);
}
