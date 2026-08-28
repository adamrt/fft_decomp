#include "fft/event.h"
#include "fft/wldcore.h"
#include "psx/types.h"

void wldcore_opcode_start_text_thread(void) {
    s32 operand;

    operand = g_wldcore_active_saved_record.instruction.bytes.operand_0;
    world_thread_start(14, world_text_character_handling_thread);
    world_thread_set_parameters(14, 9, operand | 0x8800, 0);
    g_wldcore_active_saved_record.text_id = operand;
    g_wldcore_active_saved_record.state_flags |= 4;
}
