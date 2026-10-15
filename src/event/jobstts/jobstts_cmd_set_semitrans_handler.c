#include "fft/event_jobstts.h"

menu_instruction_t* jobstts_cmd_set_semitrans_handler(menu_instruction_t* data) {
    g_jobstts_gfx_semitransparency = data->draw_state.value;
    return menu_next_instruction(data);
}
