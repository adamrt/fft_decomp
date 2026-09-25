#include "fft/battle.h"

/* Target 0x80143b80. */
void battle_script_run_next_event(void) {
    battle_script_load_next_event();
    battle_script_execute_event();
}
