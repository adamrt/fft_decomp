#include "fft/main_runtime.h"
#include "fft/main_sound.h"

void main_system_stop_display_and_audio_for_game_reset(void) {
    DrawSync(0);
    DrawSync(0);
    SetDispMask(0);
    main_sound_quit();
    SpuQuit();
}
