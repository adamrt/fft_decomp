#include "fft/event_bunit.h"
#include "psx/types.h"

/* BUNIT.OUT 001c360c - True while a menu transition (thread 15), event mode, or
 * thread 5 is running; used to gate input processing. */
s32 bunit_menu_is_active_or_transitioning(void) {
    s32 result = 0;
    if (battle_thread_is_running(0xF) || g_event_mode != 0 || (result = 0, battle_thread_is_running(5))) {
        result = 1;
    }
    return result;
}
