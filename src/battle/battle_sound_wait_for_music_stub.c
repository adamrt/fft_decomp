#include "fft/main_sound.h"
#include "fft/thread.h"
#include "psx/types.h"

void battle_sound_wait_for_music_stub(void) {
    do {
        VSync(0);
        g_battle_thread_call_target = main_return_zero_80043708;
    } while (battle_thread_call_on_main_stack() != 0);
}
