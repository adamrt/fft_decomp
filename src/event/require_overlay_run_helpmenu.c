#include "fft/event_helpmenu.h"
#include "fft/event_require.h"
#include "fft/menu.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Thread body that runs the HELPMENU overlay from the REQUIRE screen: waits
 * until 0x10000 bytes can be reserved at the high overlay address, loads the
 * overlay and runs helpmenu_menu_run_require_help in the first idle slot,
 * with the panel's two animation flags set while it runs. */
void require_overlay_run_helpmenu(void) {
    s32 thread_id;

    thread_set_current_task_id(THREAD_TASK_HELP_MENU);
    g_sound_pending_sfx_id = MAIN_SFX_CONFIRM;
    g_menu_panel_status_animation[0] = 1;
    g_menu_panel_status_animation[1] = 1;
    while (
        main_heap_reserve_at(g_main_heap_high_overlay_load_address, 0x10000) != g_main_heap_high_overlay_load_address) {
        thread_yield();
    }
    menu_request_open_companion_executable(COMPANION_OVERLAY_HELPMENU);
    thread_id = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
    thread_start(thread_id, helpmenu_menu_run_require_help);
    thread_wait_until_inactive(thread_id);
    g_menu_panel_status_animation[0] = 0;
    g_menu_panel_status_animation[1] = 0;
    thread_exit_current();
}
