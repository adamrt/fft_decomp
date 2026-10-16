/* The menu command that turns semi-transparency on or off for later primitives. */

#include "fft/menu_gfx.h"
#include "fft/menu_script.h"

/* Menu script command: turn semi-transparency on or off for later primitives and continue with the
 * next command. */
menu_instruction_t* menu_cmd_set_semitrans_handler(menu_instruction_t* data) {
    g_menu_gfx_semitransparency = data->draw_state.value;
    return menu_next_instruction(data);
}
