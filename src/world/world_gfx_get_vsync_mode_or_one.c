#include "fft/world.h"

s32 world_gfx_get_vsync_mode_or_one(void) {
    if (g_menu_event_speed == 0) {
        return 1;
    }
    return g_menu_event_speed;
}
