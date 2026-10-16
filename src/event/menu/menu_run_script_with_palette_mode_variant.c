/* Running a display script in a palette mode, withholding input when dimmed. */

#include "fft/menu_input.h"
#include "fft/menu_script.h"
#include "psx/types.h"

/* A dimmed palette also withholds input from the display script. */
void menu_run_script_with_palette_mode_variant(menu_instruction_t* script, s32 input, s32 mode) {
    s32 value = input;

    if (mode != 0) {
        value = 0;
    }
    menu_set_palette_mode(mode);
    menu_cmd_run_stream_variant(script, value);
}
