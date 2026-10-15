#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

/* Starts the thread-6 banner for `message_id` (0xD811 after auto-equip) and counts `frames` calls down
 * before stopping it; returns 0 once the banner has closed. */
s32 world_menu_run_unit_status_banner_countdown(s32 message_id, s32 frames) {
    s8 current;

    if (g_world_unit_status_banner_state == 0) {
        g_world_unit_status_banner_state = frames;
        g_world_unit_status_banner_menu.text_id = message_id;
        thread_start(THREAD_SCREEN_SLOT_MENU, g_world_unit_status_banner_menu.thread_entry);
        thread_set_parameters(THREAD_SCREEN_SLOT_MENU, (s32)&g_world_unit_status_banner_menu, 0, 0);
        g_world_unit_status_banner_active = 1;
    }
    current = g_world_unit_status_banner_state;
    if (current == 1) {
        menu_request_thread_stop(THREAD_SCREEN_SLOT_MENU);
        if (thread_is_running(THREAD_SCREEN_SLOT_MENU) == 0) {
            g_world_unit_status_banner_active = 0;
            g_world_unit_status_banner_state = 0;
            return 0;
        }
        return 1;
    }
    g_world_unit_status_banner_state = current - 1;
    return 1;
}
