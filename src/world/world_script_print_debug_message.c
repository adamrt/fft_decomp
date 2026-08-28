#include "fft/thread.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

void world_script_print_debug_message(void) {
    FntPrint(g_world_script_pause_message);
    world_thread_yield();
    g_world_script_debug_paused = 1;
}
