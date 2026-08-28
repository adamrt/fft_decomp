#include "fft/main_sound.h"
#include "fft/world.h"
#include "psx/types.h"

void world_sound_wait_for_music_stub(void) {
    do {
        VSync(0);
        g_world_thread_inner_subroutine_callback = (void (*)(void))main_return_zero_80043708;
    } while (world_thread_call_on_main_stack() != 0);
}
