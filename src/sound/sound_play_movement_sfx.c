#include "fft/battle_unit.h"
#include "fft/main.h"
#include "fft/sound.h"

/* Play a sound on a free SFX channel pair unless the unit's motion flags suppress
 * its sounds. The unit sequence scripts, the jump and landing steps and the death
 * animations play their sounds through it. */
void sound_play_movement_sfx(battle_unit_t* unit, s32 sound_id) {
    if ((unit->movement.suppress_sfx_and_landing_effects) == 0) {
        main_sound_play_sfx_find_channel(sound_id);
    }
}
