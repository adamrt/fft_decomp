#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/event_small.h"
#include "fft/map.h"
#include "fft/menu.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Request SMALL without waiting for a pending companion load to finish. */
void menu_request_open_small(void) {
    g_menu_companion_executable_request_index = COMPANION_OVERLAY_SMALL;
}

/* Drive a pending companion overlay load during map initialization. Returns 1
 * while the request is still pending; once it completes the SMALL.OUT text
 * pointers are bound and it returns 0. With no request it returns 0 at once. */
s32 menu_poll_small_load(void) {
    if (g_menu_companion_executable_request_index == 0) {
        return 0;
    }
    menu_open_companion_executable();
    if (g_menu_companion_executable_request_index != 0) {
        return 1;
    }
    small_text_init_battle_pointers();
    return 0;
}

/* Returns 0 in BATTLE and WORLD; in BATTLE it sits between
 * menu_poll_small_load and map_data_load_complete_thread. Nothing on the disc
 * calls it or points at it; the name is a neutral placeholder until a
 * reference turns up. */
s32 map_return_zero_1(void) {
    return 0;
}

/* Mark map-data loading complete after one yield, then exit after another. */
void map_data_load_complete_thread(void) {
    thread_yield();
    map_mark_data_load_complete();
    thread_yield();
    thread_exit_current();
}
