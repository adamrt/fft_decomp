#include "fft/event_require.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Thread body of display condition "ready": runs the hovered-unit stats
 * display thread for 60 frames, or until Circle, then stops it. */
void require_condition_show_ready_bugged(void) {
    s32 thread_id = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);

    thread_start(thread_id, menu_refresh_hovered_unit_stats_display);
    thread_set_parameters(thread_id, 0, 0, 0);
    require_input_wait_frames_or_skip(0x3c);
    thread_set_parameters(thread_id, 0, 0, 1);
    thread_wait_until_inactive(thread_id);
    thread_exit_current();
}
