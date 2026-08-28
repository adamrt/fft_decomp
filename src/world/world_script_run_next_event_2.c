#include "fft/world.h"
#include "psx/types.h"

/* Target 0x800f6ec8. */
void world_script_run_next_event_2(void) {
    world_script_load_next_event();
    world_script_execute_event();
}
