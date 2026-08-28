#include "fft/wldcore.h"
#include "psx/types.h"

void wldcore_opcode_end_section(void) {
    u16* flags;

    flags = &g_wldcore_active_saved_record.state_flags;
    *flags |= 0x0a;
}
