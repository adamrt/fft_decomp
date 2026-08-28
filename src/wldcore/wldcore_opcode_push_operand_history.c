#include "fft/wldcore.h"
#include "psx/types.h"

/* Shift the four-entry history g_wldcore_sound_novel_text_history_0..66 down one slot and push
 * g_wldcore_active_saved_record.instruction.bytes.operand_0 at the end (same shape as
 * wldcore_opcode_start_script_thread_and_push_history). */
/* Target 0x8008cb30. */
void wldcore_opcode_push_operand_history(void) {
    u16 history_1;
    u8 new_entry;
    u16 history_2;
    u16 history_3;

    history_1 = g_wldcore_sound_novel_text_history_1;
    new_entry = g_wldcore_active_saved_record.instruction.bytes.operand_0;
    history_2 = g_wldcore_sound_novel_text_history_2;
    history_3 = g_wldcore_sound_novel_text_history_3;
    g_wldcore_sound_novel_text_history_0 = history_1;
    g_wldcore_sound_novel_text_history_1 = history_2;
    g_wldcore_sound_novel_text_history_2 = history_3;
    g_wldcore_sound_novel_text_history_3 = new_entry;
    g_wldcore_state_flags |= 4;
}
