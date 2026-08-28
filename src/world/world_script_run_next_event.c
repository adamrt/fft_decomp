#include "fft/world.h"
#include "psx/types.h"

/* Target 0x800f6ef0. */
void world_script_run_next_event(void) {
    world_script_load_next_event();
    world_script_execute_event();
}
