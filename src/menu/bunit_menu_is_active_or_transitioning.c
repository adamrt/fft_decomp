#include "fft/event_bunit.h"
#include "fft/thread.h"
#include "psx/types.h"

/* BUNIT.OUT 001c360c - True while a menu transition (thread 15), event mode, or
 * thread 5 is running; used to gate input processing. */
s32 bunit_menu_is_active_or_transitioning(void) {
    s32 result = 0;
    if (thread_is_running(BUNIT_THREAD_SLOT_REORDER_MENU) || g_menu_input_lock_mode != 0
        || (result = 0, thread_is_running(BUNIT_THREAD_SLOT_MENU))) {
        result = 1;
    }
    return result;
}
