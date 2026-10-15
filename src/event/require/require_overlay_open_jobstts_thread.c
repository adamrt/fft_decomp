#include "fft/event_require.h"
#include "fft/thread.h"

void require_overlay_open_jobstts_thread(void) {
    thread_start(6, require_overlay_open_jobstts);
    thread_wait_until_inactive(6);
    thread_exit_current();
}
