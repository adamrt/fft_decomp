#include "fft/wldcore.h"
#include "psx/types.h"

void wldcore_opcode_subtract_counter_delta(void) {
    u16* target;
    u32 word;
    u16 flags;

    target = (u16*)&g_wldcore_active_saved_record.counter_delta;
    word = g_wldcore_opcode_state.instruction.word;
    flags = g_wldcore_active_saved_record.state_flags;
    *target -= (word << 8) >> 16;
    g_wldcore_active_saved_record.state_flags = flags | 4;
}
