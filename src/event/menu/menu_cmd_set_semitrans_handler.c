#include "fft/menu.h"
#include "fft/menu_script.h"

menu_instruction_t* menu_cmd_set_semitrans_handler(menu_instruction_t* data) {
    g_menu_gfx_semitransparency = data->draw_state.value;
    return menu_next_instruction(data);
}
