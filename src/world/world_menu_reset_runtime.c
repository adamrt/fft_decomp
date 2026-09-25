#include "fft/world.h"
#include "psx/types.h"

/* Target 0x800f2998. */
void world_menu_reset_runtime(void) {
    world_script_seed_event_rng_from_vsync();
    world_menu_clear_window_buffer_pointers();
    world_gfx_clear_texture_allocation_grid();
    world_menu_reset_state_arrays();
    world_thread_reset_scheduler();
}
