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
