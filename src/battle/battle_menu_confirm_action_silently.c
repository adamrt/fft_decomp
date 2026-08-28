#include "fft/battle.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Runs the menu's action handler on a simulated, silent Circle press with
 * option 0 selected, then clears the injected input. */
void battle_menu_confirm_action_silently(void* menu) {
    g_battle_sound_suppressed = 1;
    g_battle_script_event_input = PSX_PAD_CIRCLE;
    battle_menu_handle_action(menu, 0);
    g_battle_sound_suppressed = 0;
    g_battle_script_event_input = 0;
}
