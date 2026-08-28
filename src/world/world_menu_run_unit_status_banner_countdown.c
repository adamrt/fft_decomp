#include "fft/world.h"
#include "psx/types.h"

/* Starts the thread-6 banner for `message_id` (0xD811 after auto-equip) and counts `frames` calls down
 * before stopping it; returns 0 once the banner has closed. */
s32 world_menu_run_unit_status_banner_countdown(s32 message_id, s32 frames) {
    s8 current;
    s32 unused;

    if (g_world_unit_status_banner_state == 0) {
        g_world_unit_status_banner_state = frames;
        g_world_unit_status_banner_menu.text_id = message_id;
        world_thread_start(6, g_world_unit_status_banner_menu.thread_entry);
        world_thread_set_parameters(6, (s32)&g_world_unit_status_banner_menu, 0, 0);
        g_world_unit_status_banner_active = 1;
    }
    current = g_world_unit_status_banner_state;
    if (current == 1) {
        world_menu_stop_unit_status_banner_thread(6);
        if (world_thread_is_running(6) == 0) {
            g_world_unit_status_banner_active = 0;
            g_world_unit_status_banner_state = 0;
            return 0;
        }
        return 1;
    }
    g_world_unit_status_banner_state = current - 1;
    return 1;
}
