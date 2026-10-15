#include "fft/script.h"

/* Keep the raw 32-bit button value in g_script_unfiltered_controller_input, then
 * apply the script's disabled and forced inputs to it in place. */
void script_filter_controller_input(u32* input) {
    g_script_unfiltered_controller_input = *input;
    script_apply_input_overrides(input);
}
