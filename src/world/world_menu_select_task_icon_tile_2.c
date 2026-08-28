#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

void world_menu_select_task_icon_tile_2(SPRT* slot) {
    s32 icon;

    if (world_thread_is_previous_running() != 0 || g_world_thread_task_active == 1) {
        icon = 0x7C7C;
    } else {
        icon = 0x7CBC;
    }
    slot->clut = icon;
}
