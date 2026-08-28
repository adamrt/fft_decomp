#include "fft/world.h"
#include "psx/types.h"

void world_script_filter_controller_input(s32* src) {
    g_world_input_unfiltered_controller = *src;
    world_menu_refresh_state_from_script_variables(src);
}
