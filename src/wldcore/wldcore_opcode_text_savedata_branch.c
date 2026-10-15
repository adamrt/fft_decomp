/* Text thread start, random branch and save-data bit branches. */

#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/types.h"

/* Sound-novel opcode: show text operand 0 (text id 0x8800 + operand) in the
 * message thread with dialog type 9, and record it as the current text. */
void wldcore_opcode_start_text_thread(void) {
    s32 operand;

    operand = (u8)(g_wldcore_active_saved_record.instruction >> 8);
    thread_start(WLDCORE_THREAD_SLOT_MESSAGE, text_run_dialog_thread_variant);
    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 9, operand | TEXT_ID_WORLD_MESSAGE_BASE, 0);
    g_wldcore_active_saved_record.text_id = operand;
    g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_FETCH;
}

/* Sound-novel opcode: branch to one of the three sections named by the
 * operand bytes, chosen by rand() with equal weight. */
void wldcore_opcode_branch_to_random_section(void) {
    s32 pick;
    s32 value;

    pick = (rand() * 3) >> 15;
    if (pick == 0) {
        value = (u8)(g_wldcore_active_saved_record.instruction >> 8);
    }
    if (pick == 1) {
        value = (u8)(g_wldcore_active_saved_record.instruction >> 16);
    }
    if (pick == 2) {
        value = (u8)(g_wldcore_active_saved_record.instruction >> 24);
    }
    g_wldcore_active_saved_record.section = value;
    g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_BRANCH;
}

void wldcore_opcode_branch_if_savedata_bit_set(void) {
    wldcore_opcode_branch_if_savedata_bit(1);
}

/* Script branch on a save-data bit selected by a 16-bit operand: when the
 * bit equals the expected value the branch target byte is armed at 0x800d484e
 * with state flag 0x02, otherwise only flag 0x04 is raised. */
void wldcore_opcode_branch_if_savedata_bit(s32 expected) {
    u32* instruction = &g_wldcore_opcode_state.instruction;
    u16 flags;

    if (wldcore_test_bit(g_main_saved_data_bits, ((u8)(*instruction >> 8) << 8) | (u8)(*instruction >> 16))
        == expected) {
        flags = g_wldcore_active_saved_record.state_flags | WLDCORE_OPCODE_STATE_BRANCH;
        g_wldcore_active_saved_record.section = (u8)(*instruction >> 24);
    } else {
        flags = g_wldcore_active_saved_record.state_flags | WLDCORE_OPCODE_STATE_FETCH;
    }
    g_wldcore_active_saved_record.state_flags = flags;
}

void wldcore_opcode_branch_if_savedata_bit_clear(void) {
    wldcore_opcode_branch_if_savedata_bit(0);
}
