#include "fft/event_require.h"
#include "fft/thread.h"

void require_thread_stop_after_16_frames(void) {
    thread_wait_frames(0x10);
    require_thread_exit_current();
}
