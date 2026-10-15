#include "fft/battle_effect.h"
#include "fft/battle_gfx.h"
#include "fft/battle_unit.h"

/* Returns the next sound of an effect sound channel, chosen from the channel's
 * three ids by its mode and the number of sounds it has played; 0 is no sound.
 *
 * Modes above 4 fall through with an unassigned result, as in the target. */
s32 effect_select_sound_id(s32 channel) {
    s32 count;
    s32 sound;

    count = g_effect_sound_call_counts[channel]++;
    switch (g_effect_flags_section->channels[channel].mode) {
    case EFFECT_SOUND_MODE_FIRST:
        sound = g_effect_flags_section->channels[channel].sound_ids[0];
        break;
    case EFFECT_SOUND_MODE_ALTERNATE:
        sound = g_effect_flags_section->channels[channel].sound_ids[count & 1];
        break;
    case EFFECT_SOUND_MODE_FIRST_THEN_SECOND:
        if (count == 0) {
            sound = g_effect_flags_section->channels[channel].sound_ids[0];
        } else {
            sound = g_effect_flags_section->channels[channel].sound_ids[1];
        }
        break;
    case EFFECT_SOUND_MODE_FIRST_THEN_ALTERNATE:
        if (count != 0) {
            sound = g_effect_flags_section->channels[channel].sound_ids[1 + (count & 1)];
        } else {
            sound = g_effect_flags_section->channels[channel].sound_ids[0];
        }
        break;
    case EFFECT_SOUND_MODE_CYCLE:
        sound = g_effect_flags_section->channels[channel].sound_ids[count % 3];
        break;
    }
    return sound;
}
