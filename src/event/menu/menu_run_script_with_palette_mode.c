/* Running a display script in a palette mode, withholding input when dimmed. */

#include "fft/menu_input.h"
#include "fft/menu_script.h"
#include "psx/types.h"

/* Run a menu display script with palette mode `mode` (nonzero dims it); a dimmed script also gets
 * no input. */
void menu_run_script_with_palette_mode(menu_instruction_t* script, s32 input, s32 mode) {
    s32 value = input;

    if (mode != 0) {
        value = 0;
    }
    menu_set_palette_mode(mode);
    menu_cmd_run_stream(script, value);
}
