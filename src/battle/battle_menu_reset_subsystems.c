#include "fft/battle.h"
#include "psx/types.h"

void battle_menu_reset_subsystems(void) {
    battle_script_seed_random_from_vsync();
    battle_menu_reset_buffer_slots();
    battle_gfx_clear_tpage7_vram_allocation_grid();
    battle_menu_init_buffer_allocator();
    battle_thread_reset_scheduler();
}
