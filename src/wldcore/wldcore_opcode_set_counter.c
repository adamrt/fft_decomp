#include "fft/wldcore.h"
#include "psx/types.h"

void wldcore_opcode_set_counter(void) {
    u32 word;
    u16 flags;

    word = g_wldcore_opcode_state.instruction.word;
    flags = g_wldcore_state_flags;
    g_wldcore_active_saved_record.counter = (word << 8) >> 16;
    g_wldcore_state_flags = flags | 4;
}
