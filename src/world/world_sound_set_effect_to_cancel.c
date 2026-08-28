#include "fft/main_sound.h"
#include "fft/world.h"
#include "psx/types.h"

void world_sound_set_effect_to_cancel(void) {
    g_world_sound_effect_id_to_play = MAIN_SFX_CANCEL;
}
