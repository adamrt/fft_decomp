#include "fft/thread.h"
#include "fft/wldcore.h"

void wldcore_opcode_wait_for_text_thread(void) {
    if (world_thread_is_running(0xE) == 0) {
        g_wldcore_active_saved_record.state_flags |= 4;
    } else {
        g_wldcore_active_saved_record.state_flags |= 8;
    }
}
