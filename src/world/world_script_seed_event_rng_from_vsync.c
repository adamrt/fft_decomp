#include "fft/world.h"
#include "psx/types.h"

void world_script_seed_event_rng_from_vsync(void) {
    g_world_event_random_seed = VSync(-1);
}
