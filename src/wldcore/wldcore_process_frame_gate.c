#include "fft/main_runtime.h"
#include "fft/wldcore.h"

/* Reinitialize WLDCORE when system-flag bit 1 transitions from set to clear. */
void wldcore_process_frame_gate(void) {
    if ((g_main_system_flags & 2) == 0 && (g_wldcore_previous_system_flags & 2) != 0) {
        wldcore_init_core();
    }
    g_wldcore_previous_system_flags = g_main_system_flags;
}
