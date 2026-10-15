#include "fft/world.h"
#include "psx/types.h"

void world_menu_set_draw_priority(s32 priority) {
    g_menu_gfx_otag_index = priority;
}
