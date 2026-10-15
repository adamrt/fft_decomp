#include "fft/world.h"
#include "psx/types.h"

void world_menu_clear_cursor_positions(void) {
    s32 i;

    for (i = 15; i >= 0; i--) {
        g_menu_selection_values[i] = 0;
    }
}
