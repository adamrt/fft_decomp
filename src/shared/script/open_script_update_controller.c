#include "fft/open.h"

/* Handler 10: run the ending script for a frame. When it ends, VSync mode 0 is
 * restored, the controller is popped and OPEN.BIN's loop ends. */
void open_script_update_controller(open_controller_record_t* unused_record) {
    if (open_script_init_and_dispatch_entries() == 0) {
        g_open_system_vsync_mode = 0;
        g_open_current_controller_index--;
        g_open_system_runtime_flags ^= OPEN_SYSTEM_RUNTIME_FLAG_RUNNING;
    }
}
