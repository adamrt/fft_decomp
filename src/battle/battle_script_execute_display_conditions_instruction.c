#include "fft/battle.h"
#include "psx/types.h"

/* Target 0x8013bd6c. */
void battle_script_execute_display_conditions_instruction(void) {
    g_battle_menu_hide_numeric_values = 0;
    require_condition_dispatch();
}
