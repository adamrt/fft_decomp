#include "fft/world.h"
#include "psx/types.h"

void world_menu_set_main_scroll_parameters(s32 direction, s32 threshold) {
    g_world_menu_main_scroll_direction = direction;
    g_world_menu_main_scroll_threshold = threshold;
}
