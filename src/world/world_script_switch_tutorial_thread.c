#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

void world_script_switch_tutorial_thread(void) {
    g_world_script_tutorial_thread_switch_counter = 0xff;
    do {
        world_thread_yield();
        g_world_script_tutorial_thread_switch_counter -= 4;
    } while (g_world_script_tutorial_thread_switch_counter > 0);
    g_world_script_tutorial_thread_switch_counter = 0;
}
