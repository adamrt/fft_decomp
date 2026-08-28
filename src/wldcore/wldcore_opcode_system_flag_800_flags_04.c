#include "fft/main_runtime.h"
#include "fft/wldcore.h"
#include "psx/types.h"

void wldcore_opcode_system_flag_800_flags_04(void) {
    u16* flags;
    u32 sys;
    u16 value;

    flags = &g_wldcore_active_saved_record.state_flags;
    sys = g_main_system_flags;
    value = *flags;
    g_main_system_flags = sys | 0x800;
    *flags = (value & 0xFF7F) | 4;
}
