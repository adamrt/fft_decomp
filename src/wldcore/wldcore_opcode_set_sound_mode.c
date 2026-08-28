#include "fft/wldcore.h"
#include "psx/types.h"

void wldcore_opcode_set_sound_mode(void) {
    u16 flags;

    flags = g_wldcore_state_flags | 4;
    g_wldcore_active_saved_record.sound_mode = g_wldcore_active_saved_record.instruction.bytes.operand_0;
    g_wldcore_state_flags = flags;
}
