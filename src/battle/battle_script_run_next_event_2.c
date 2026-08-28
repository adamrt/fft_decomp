#include "fft/battle.h"
#include "fft/event.h"

/* Target 0x80143ba8. */
void battle_script_run_next_event_2(void) {
    battle_script_load_next_event();
    battle_script_execute_event();
}
