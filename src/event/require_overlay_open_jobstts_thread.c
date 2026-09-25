#include "fft/event_require.h"

void require_overlay_open_jobstts_thread(void) {
    battle_thread_start(6, require_overlay_open_jobstts);
    battle_thread_wait_until_inactive(6);
    battle_thread_exit_current();
}
