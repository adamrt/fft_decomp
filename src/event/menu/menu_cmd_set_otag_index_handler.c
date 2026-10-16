#include "fft/menu.h"
#include "fft/menu_script.h"

menu_instruction_t* menu_cmd_set_otag_index_handler(menu_instruction_t* data) {
    g_menu_gfx_otag_index = data->draw_state.value;
    return menu_next_instruction(data);
}
