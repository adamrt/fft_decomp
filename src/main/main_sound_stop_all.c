#include "fft/main_sound.h"
#include "psx/types.h"

void main_sound_stop_all(void) {
    main_smd_stop_marked_music();
    SuzukiTurnOffAllMusic();
}
