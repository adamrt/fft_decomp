#include "fft/event_require.h"
#include "psx/types.h"

void require_sound_wait_music_idle(void) {
    do {
        VSync(0);
        g_battle_thread_call_target = (void (*)(void))main_return_zero_80043708;
    } while (battle_thread_call_on_main_stack() != 0);
}
