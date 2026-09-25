#include "fft/battle.h"
#include "psx/types.h"

void battle_sound_set_effect_to_confirm(void) {
    g_sound_effect_id_to_play = MAIN_SFX_CONFIRM;
}
