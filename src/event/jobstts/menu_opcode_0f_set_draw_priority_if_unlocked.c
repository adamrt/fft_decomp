/* The menu command that selects the ordering-table entry for later primitives unless it is locked. */

#include "fft/gfx_primitive.h"
#include "fft/menu_gfx.h"
#include "fft/menu_script.h"

/* Menu script command: select the ordering-table slot for later primitives unless the slot is
 * locked, and continue with the next command. */
menu_instruction_t* menu_opcode_0f_set_draw_priority_if_unlocked(menu_instruction_t* data) {
    if (g_gfx_otag_index_locked == 0) {
        g_menu_gfx_otag_index = data->draw_state.value;
    }
    return menu_next_instruction(data);
}
