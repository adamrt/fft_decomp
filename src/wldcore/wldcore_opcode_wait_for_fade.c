#include "fft/wldcore.h"
#include "psx/types.h"

void wldcore_opcode_wait_for_fade(void) {
    u16* flags;

    if ((g_main_system_flags & 8) == 0) {
        flags = &g_wldcore_active_saved_record.state_flags;
        *flags |= 4;
    } else {
        flags = &g_wldcore_active_saved_record.state_flags;
        *flags |= 8;
    }
}
