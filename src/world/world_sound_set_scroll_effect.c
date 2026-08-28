#include "fft/main_sound.h"
#include "fft/world.h"
#include "psx/types.h"

void world_sound_set_scroll_effect(void) {
    g_world_sound_effect_id_to_play = MAIN_SFX_CURSOR_MOVE;
}
