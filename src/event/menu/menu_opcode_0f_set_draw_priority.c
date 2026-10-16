/* The menu command that selects the ordering-table entry for later primitives. */

#include "fft/menu_gfx.h"
#include "fft/menu_script.h"

/* Menu script command: select the ordering-table slot for later primitives and continue with the
 * next command. */
menu_instruction_t* menu_opcode_0f_set_draw_priority(menu_instruction_t* data) {
    g_menu_gfx_otag_index = data->draw_state.value;
    return menu_next_instruction(data);
}
