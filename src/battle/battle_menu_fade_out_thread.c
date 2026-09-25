#include "fft/battle.h"
#include "psx/types.h"

void battle_menu_fade_out_thread(void) {
    s32 thread_id;
    s32 fade;
    s32 frame_padding[2]; /* Unused; reproduces the target's 8 extra frame bytes. */

    g_battle_menu_transition_state = 4;
    do {
        battle_thread_yield();
    } while (battle_thread_is_running_8014cc94(6));

    g_companion_overlay_state = 2;
    g_battle_menu_help_open = 0;
    do {
        battle_thread_wait_frames(1);
    } while (g_companion_overlay_state != 0);

    battle_menu_free_high_overlay();
    for (thread_id = 2; thread_id < 15; thread_id++) {
        battle_thread_set_parameters(thread_id, 0, 0, 1);
    }

    for (fade = 0; fade < 256; fade += 8) {
        g_battle_screen_fade = 0xff - fade;
        battle_thread_yield();
    }

    battle_menu_clear_option_menu_open();
    g_option_menu_submenu_state = 0;
    g_battle_screen_fade = 0;
    g_battle_menu_transition_state = 0;
    battle_menu_store_unit_names_and_event_block_data(2, g_battle_active_turn_unit.battle_id, 0xff);
    battle_thread_exit_current();
}
