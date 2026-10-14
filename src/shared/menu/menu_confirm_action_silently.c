#include "fft/battle_menu.h"
#include "fft/battle_script.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "psx/pad.h"

/* Runs the menu's action handler on a simulated, silent Circle press with
 * option 0 selected, then clears the injected input. */
void menu_confirm_action_silently(menu_entry_t* menu) {
    g_sound_suppressed = 1;
    g_script_event_input = PSX_PAD_CIRCLE;
    menu_handle_action(menu, 0);
    g_sound_suppressed = 0;
    g_script_event_input = 0;
}
