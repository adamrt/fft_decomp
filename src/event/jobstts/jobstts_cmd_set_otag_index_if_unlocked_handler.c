#include "fft/event_jobstts.h"
#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/menu_script.h"

menu_instruction_t* jobstts_cmd_set_otag_index_if_unlocked_handler(menu_instruction_t* data) {
    if (g_gfx_otag_index_locked == 0) {
        g_menu_gfx_otag_index = data->draw_state.value;
    }
    return menu_next_instruction(data);
}
