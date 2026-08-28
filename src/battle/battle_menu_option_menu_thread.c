#include "fft/battle.h"
#include "psx/types.h"

/* Thread body that swaps in companion overlay 7 behind the option menu. */
void battle_menu_option_menu_thread(void) {
    if (battle_thread_is_running_8014cc94(3) != 0) {
        battle_thread_exit_current();
    }
    if (battle_thread_is_running_8014cc94(1) != 0) {
        battle_thread_exit_current();
    }
    battle_menu_set_option_menu_open();
    battle_sound_set_effect_to_confirm();
    battle_menu_retry_alloc_with_message(0x20000);
    battle_menu_request_open_companion_executable(7);
    battle_thread_wait_frames(2);
    g_companion_overlay_state = 3;
    do {
        g_battle_script_event_input = 0;
        battle_thread_yield();
    } while (g_companion_overlay_state != 0);
    g_battle_script_event_input = 0;
    battle_menu_free_high_overlay();
    battle_thread_wait_frames(2);
    battle_menu_clear_option_menu_open();
    battle_thread_exit_current();
}
