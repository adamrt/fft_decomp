#include "fft/world.h"
#include "psx/types.h"

void world_menu_start_main_mode_2(void) {
    world_menu_set_transition_active_flag();
    world_bin_load_file(1);
    world_menu_run_main_mode(2);
}
