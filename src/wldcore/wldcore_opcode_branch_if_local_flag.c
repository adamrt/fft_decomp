#include "fft/wldcore.h"
#include "psx/types.h"

/* Script branch on a bit of the world-local bit array at 0x800d4564 selected
 * by the single operand byte: when the bit equals the expected value the
 * branch target byte is armed at 0x800d484e with state flag 0x02, otherwise
 * only flag 0x04 is raised.
 *
 * The target keeps the instruction record address in $s1 across the
 * wldcore_test_bit call instead of rebuilding it at each byte load. GCC folds
 * a symbol address straight into both `lbu`s, so the empty tied `__asm__`
 * (which emits no instruction) is what forces the base into its own pseudo;
 * no plain C spelling of the two loads reproduces it. */
void wldcore_opcode_branch_if_local_flag(s32 expected) {
    wldcore_opcode_instruction_bytes_t* instruction = &g_wldcore_opcode_state.instruction.bytes;
    u16 flags;

    __asm__("" : "=r"(instruction) : "0"(instruction));

    if (wldcore_test_bit(g_wldcore_sound_novel_local_flags, instruction->operand_0) == expected) {
        flags = g_wldcore_active_saved_record.state_flags | 2;
        g_wldcore_active_saved_record.section = instruction->operand_1;
    } else {
        flags = g_wldcore_active_saved_record.state_flags | 4;
    }
    g_wldcore_active_saved_record.state_flags = flags;
}
