#include "fft/script.h"
#include "fft/thread.h"
#include "fft/world.h"

/* Menu action: request the save/load screen from the world map, hand the
 * formation input state to the event input and exit the thread. */
void world_card_open_menu(void) {
    g_world_card_menu_request_state = WORLD_SCREEN_REQUEST_START_FADE_OUT;
    g_world_formation_menu_new_buttons = 0;
    script_load_event_input_state(&g_world_formation_menu_input_state);
    thread_exit_current();
}
