#include "fft/battle.h"
#include "psx/types.h"

void battle_menu_input_thread(void) {
    void* menu;

    menu = (void*)battle_thread_get_current_parameter_1();
    g_battle_sound_suppressed = 1;
    while (1) {
        battle_thread_yield();
        if (battle_menu_should_close_thread(&g_battle_script_event_input) != 0) {
            break;
        }
        battle_menu_handle_action(menu, 0);
        battle_handle_menu_cancel_input(menu);
    }
    battle_thread_yield();
    g_battle_sound_suppressed = 0;
    if (battle_thread_get_current_parameter_2() == 0) {
        battle_thread_exit_current();
    }
}
