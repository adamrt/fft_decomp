#include "fft/battle.h"

void battle_sound_play_movement_sfx(battle_unit_misc_data_t* unit, s32 sound_id) {
    if ((unit->movement.word & BATTLE_MOTION_FLAG_SUPPRESS_SFX_AND_LANDING_EFFECTS) == 0) {
        main_sound_play_sfx_find_channel(sound_id);
    }
}
