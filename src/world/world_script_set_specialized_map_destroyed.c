#include "fft/world.h"
#include "psx/types.h"

void world_script_set_specialized_map_destroyed(void) {
    s16 value = g_world_map_destruction_wait_state;

    if (value == 1) {
        g_world_map_destruction_wait_state = value + 1;
    }
}
