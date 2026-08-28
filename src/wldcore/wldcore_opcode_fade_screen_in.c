#include "fft/wldcore.h"
#include "psx/types.h"

void wldcore_opcode_fade_screen_in(void) {
    u32* state = (u32*)&g_wldcore_screen_fade_state;
    u16* flags = &g_wldcore_active_saved_record.state_flags;

    *state |= 2;
    *flags &= ~0x100;
    wldcore_fade_start_screen(0, g_wldcore_active_saved_record.instruction.bytes.operand_0);
    *flags |= 4;
}
