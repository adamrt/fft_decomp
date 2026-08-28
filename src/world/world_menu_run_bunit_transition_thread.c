#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

void world_menu_run_bunit_transition_thread(void) {
    s32 level;

    for (level = 0; level < 0x100; level += 0x10) {
        g_world_screen_fade = level;
        world_thread_yield();
    }
    g_world_screen_fade = 0xFF;
    g_world_menu_overlay_state = 1;
    world_thread_set_parameters(8, 0, 0, 1);
    world_thread_set_parameters(10, 0, 0, 1);
    world_thread_set_parameters(11, 0, 0, 1);
    world_thread_set_parameters(12, 0, 0, 1);
    world_thread_set_parameters(13, 0, 0, 1);
    world_thread_wait_for_10_to_13();
    world_bin_load_file(2);
    world_menu_retry_alloc_with_message(0x20000);
    world_bin_load_file(0xF);
    world_thread_wait_frames(2);
    g_world_text_overlay_active = 1;
    world_thread_start(13, world_menu_fade_out_thread);
    world_thread_exit_current();
}
