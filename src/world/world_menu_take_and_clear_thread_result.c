#include "fft/world.h"
#include "psx/types.h"

s32 world_menu_take_and_clear_thread_result(void) {
    s32 result = g_world_menu_thread_result;

    g_world_menu_thread_result = 0;
    return result;
}
