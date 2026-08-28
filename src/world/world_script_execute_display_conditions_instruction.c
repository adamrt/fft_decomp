#include "fft/event.h"
#include "fft/world.h"
#include "psx/types.h"

/* Target 0x800ef984; the no-op immediately before it is a separate function. */
void world_script_execute_display_conditions_instruction(void) {
    g_world_menu_hide_numeric_values = 0;
    require_condition_dispatch();
}
