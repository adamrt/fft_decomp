#include "fft/world.h"
#include "psx/types.h"

void world_menu_preview_ability_turn_at_list(void) {
    world_menu_build_ability_preview_at_list();
    world_script_pulse_tutorial_wait_value(0xFA);
    world_menu_scrolling_list_thread();
}
