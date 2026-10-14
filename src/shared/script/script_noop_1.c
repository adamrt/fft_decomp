#include "fft/script.h"
#include "psx/types.h"

/* Empty in BATTLE and WORLD; in BATTLE it sits between
 * menu_request_open_companion_data_file and
 * script_execute_display_conditions_instruction. Nothing on the disc calls it
 * or points at it; the name is a neutral placeholder until a reference turns
 * up. */
void script_noop_1(void) {
}
