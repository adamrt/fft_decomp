#include "fft/script.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Wait until the unit view's four panel threads (slots 13 down to 10) have stopped. */
void thread_wait_for_unit_view_panels(void) {
    thread_wait_until_inactive(THREAD_SLOT_HOVERED_UNIT_STATS);
    thread_wait_until_inactive(THREAD_SLOT_RIGHT_UNIT_STATS);
    thread_wait_until_inactive(THREAD_SLOT_UNIT_SUMMARY_PANEL);
    thread_wait_until_inactive(THREAD_SLOT_TILE_INFO_PANEL);
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between
 * thread_wait_for_unit_view_panels and script_noop_5. Nothing on the disc
 * calls it or points at it; the name is a neutral placeholder until a
 * reference turns up. */
void script_noop_4(void) {
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between script_noop_4 and
 * script_frame_after_threads_stub. Nothing on the disc calls it or points at
 * it; the name is a neutral placeholder until a reference turns up. */
void script_noop_5(void) {
}

/* Empty. The event frame runner calls it each frame once the threads have run. */
void script_frame_after_threads_stub(void) {
}
