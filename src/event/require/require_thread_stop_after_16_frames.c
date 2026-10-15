#include "fft/battle_script.h"
#include "fft/event_require.h"
#include "fft/main.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "psx/pad.h"
#include "psx/types.h"

void require_thread_stop_after_16_frames(void) {
    thread_wait_frames(0x10);
    require_thread_exit_current();
}
