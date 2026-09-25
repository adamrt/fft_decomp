#include "fft/wldcore.h"
#include "psx/types.h"

/* Script branch on a bit of the second save-data word array selected by the
 * single operand byte: when the bit equals the expected value the branch
 * target byte is armed at 0x800d484e with state flag 0x02, otherwise only
 * flag 0x04 is raised. */
void wldcore_opcode_branch_if_saved_bit(s32 expected) {
    wldcore_opcode_instruction_bytes_t* instruction = &g_wldcore_opcode_state.instruction.bytes;
    u16 flags;

    /* Keep the instruction address live across the call for both operand loads. */
    __asm__("" : "=r"(instruction) : "0"(instruction));

    if (wldcore_test_bit(g_main_secondary_saved_data_bits, instruction->operand_0) == expected) {
        flags = g_wldcore_active_saved_record.state_flags | 2;
        g_wldcore_active_saved_record.section = instruction->operand_1;
    } else {
        flags = g_wldcore_active_saved_record.state_flags | 4;
    }
    g_wldcore_active_saved_record.state_flags = flags;
}
