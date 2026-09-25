#include "fft/battle.h"

/* Returns the byte offset of the next `opcode` instruction at or after
 * `offset`, or 0 when EventEnd comes first. */
s32 battle_script_find_instruction_byte_offset(s32 offset, s32 opcode) {
    /* Bytecode offsets are relative to the complete file block. */
    u8* base = (u8*)g_battle_event_block;
    s32 operands_offset;
    u8 op;
    for (;;) {
        op = base[offset];
        if (op == EVENT_OPCODE_END)
            return 0;
        if (op == opcode)
            return offset;
        operands_offset = offset + 1;
        offset = operands_offset + g_battle_script_event_instruction_sizes[op];
    }
}
