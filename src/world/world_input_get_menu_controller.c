#include "fft/world.h"
#include "psx/types.h"

u32* world_input_get_menu_controller(s32 player) {
    if (player == 0) {
        return (u32*)g_menu_controller_input_0;
    }
    return (u32*)g_menu_controller_input_1;
}
