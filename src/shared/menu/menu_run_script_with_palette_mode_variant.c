#include "fft/event_equip.h"
#include "fft/menu_script.h"

/* A dimmed palette also withholds input from the display script. */
void menu_run_script_with_palette_mode_variant(menu_instruction_t* script, s32 input, s32 mode) {
    s32 value;

    value = input;
    if (mode != 0) {
        value = 0;
    }
    menu_set_palette_mode(mode);
    menu_cmd_run_stream_variant(script, value);
}
