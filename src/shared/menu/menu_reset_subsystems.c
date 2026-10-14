#include "fft/battle_gfx.h"
#include "fft/battle_menu.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"

/* Reset what the menu layer shares: reseed the event script's random generator,
 * free every window buffer pair, the tpage-7 VRAM grid and the menu buffer arena,
 * and reset the thread scheduler. */
void menu_reset_subsystems(void) {
    script_seed_random_from_vsync();
    menu_reset_window_buffer_slots();
    battle_gfx_clear_tpage7_vram_allocation_grid();
    battle_menu_init_buffer_allocator();
    thread_reset_scheduler();
}
