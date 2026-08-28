#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

void world_menu_fade_out_thread(void) {
    s32 i;
    s32 unused[2]; /* keeps the original 0x28 frame */

    g_world_menu_transition_timeout = 4;
    do {
        world_thread_yield();
    } while (world_thread_is_running_80100164(6) != 0);

    g_world_companion_overlay_state = 2;
    g_world_text_overlay_active = 0;
    do {
        world_thread_wait_frames(1);
    } while (g_world_companion_overlay_state != 0);

    world_menu_free_high_overlay();
    for (i = 2; i < 15; i++) {
        world_thread_set_parameters(i, 0, 0, 1);
    }
    for (i = 0; i < 0x100; i += 8) {
        g_world_screen_fade = 0xFF - i;
        world_thread_yield();
    }
    world_menu_clear_transition_active_flag();
    g_world_menu_overlay_state = 0;
    g_world_screen_fade = 0;
    g_world_menu_transition_timeout = 0;
    world_unit_start_view_thread(2, g_world_unit_view_battle_id, 0xFF);
    world_thread_exit_current();
}
