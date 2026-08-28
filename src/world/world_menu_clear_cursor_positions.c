#include "fft/world.h"
#include "psx/types.h"

void world_menu_clear_cursor_positions(void) {
    s32 i;

    for (i = 15; i >= 0; i--) {
        g_world_menu_cursor_positions[i] = 0;
    }
}
