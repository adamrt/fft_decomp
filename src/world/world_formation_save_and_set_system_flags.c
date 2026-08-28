#include "fft/main_runtime.h"
#include "fft/world.h"
#include "psx/types.h"

void world_formation_save_and_set_system_flags(void) {
    g_world_formation_saved_system_flags = g_main_system_flags;
    g_main_system_flags |= 0x3e70;
}
