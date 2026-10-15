#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/types.h"

/* Start thread 14 on operand_0 | 0x8800 (as
 * wldcore_opcode_start_script_thread_flags_04 does with the opcode operand), then push
 * operand_0 onto the four-entry history
 * g_wldcore_active_saved_record.text_history_0..3. */
void wldcore_opcode_start_text_thread_and_push_history(void) {
    u16 history_1;
    u16 history_2;
    u16 history_3;
    u16 flags;
    s32 operand;

    operand = (u8)(g_wldcore_active_saved_record.instruction >> 8);
    thread_start(WLDCORE_THREAD_SLOT_MESSAGE, text_run_dialog_thread_variant);
    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 9, operand | TEXT_ID_WORLD_MESSAGE_BASE, 0);
    history_1 = g_wldcore_active_saved_record.text_history_1;
    history_2 = g_wldcore_active_saved_record.text_history_2;
    history_3 = g_wldcore_active_saved_record.text_history_3;
    flags = g_wldcore_active_saved_record.state_flags;
    g_wldcore_active_saved_record.text_id = operand;
    g_wldcore_active_saved_record.text_history_3 = operand;
    g_wldcore_active_saved_record.text_history_0 = history_1;
    g_wldcore_active_saved_record.text_history_1 = history_2;
    g_wldcore_active_saved_record.text_history_2 = history_3;
    g_wldcore_active_saved_record.state_flags = flags | WLDCORE_OPCODE_STATE_FETCH;
}
