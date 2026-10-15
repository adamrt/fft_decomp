#include "fft/script.h"
#include "fft/wldcore.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Reinitialize WLDCORE when system-flag bit 1 transitions from set to clear. */
void wldcore_process_frame_gate(void) {
    if ((g_main_system_flags & MAIN_SYSTEM_FLAG_DRAW_MAP) == 0
        && (g_wldcore_previous_system_flags & MAIN_SYSTEM_FLAG_DRAW_MAP) != 0) {
        wldcore_init_core();
    }
    g_wldcore_previous_system_flags = g_main_system_flags;
}
