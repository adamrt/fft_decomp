#include "fft/battle.h"
#include "fft/event.h"
#include "psx/types.h"

void battle_menu_run_bunit_transition_thread(void) {
    s32 fade;

    for (fade = 0; fade < 256; fade += 16) {
        g_battle_screen_fade = fade;
        battle_thread_yield();
    }
    g_battle_screen_fade = 0xff;
    g_option_menu_submenu_state = 1;
    battle_thread_set_parameters(8, 0, 0, 1);
    battle_thread_set_parameters(10, 0, 0, 1);
    battle_thread_set_parameters(11, 0, 0, 1);
    battle_thread_set_parameters(12, 0, 0, 1);
    battle_thread_set_parameters(13, 0, 0, 1);
    battle_thread_wait_for_10_to_13();
    battle_menu_request_open_companion_executable(2);
    battle_menu_retry_alloc_with_message(0x20000);
    battle_menu_request_open_companion_executable(0xf);
    battle_thread_wait_frames(2);
    g_battle_menu_help_open = 1;
    battle_thread_start(13, battle_menu_fade_out_thread);
    battle_thread_exit_current();
}
