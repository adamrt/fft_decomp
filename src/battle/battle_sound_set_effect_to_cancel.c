#include "fft/battle.h"
#include "fft/main_sound.h"
#include "psx/types.h"

void battle_sound_set_effect_to_cancel(void) {
    g_sound_effect_id_to_play = MAIN_SFX_CANCEL;
}
