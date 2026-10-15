#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"

/* Thread entry of system function 0x28 (the silent move confirmation): with menu
 * sounds suppressed, handle confirm and cancel on the menu each frame until
 * battle_menu_should_close_thread asks it to stop. The thread exits only when its
 * second parameter is 0. */
void menu_input_thread(void) {
    menu_entry_t* menu;

    menu = (menu_entry_t*)thread_get_current_parameter_1_variant();
    g_sound_suppressed = 1;
    while (1) {
        thread_yield();
        if (menu_should_close_thread(&g_script_event_input) != 0) {
            break;
        }
        menu_handle_action(menu, 0);
        menu_handle_cancel_input(menu);
    }
    thread_yield();
    g_sound_suppressed = 0;
    if (thread_get_current_parameter_2_variant() == 0) {
        thread_exit_current();
    }
}
