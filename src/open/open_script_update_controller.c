#include "fft/open.h"
#include "psx/types.h"

void open_script_update_controller(void) {
    if (open_script_init_and_dispatch_entries() == 0) {
        g_open_system_vsync_mode = 0;
        g_open_current_controller_index--;
        g_open_system_runtime_flags ^= 1;
    }
}
