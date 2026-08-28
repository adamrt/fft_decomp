#include "fft/battle_effect.h"

/* Select the sound for an effect channel from its mode and call count.
 *
 * Modes above 4 fall through with an unassigned result, as in the target. */
s32 battle_effect_get_on_hit_sound_index(s32 channel) {
    s32 count;
    s32 sound;

    count = g_battle_effect_sound_call_counts[channel]++;
    switch (g_battle_effect_flags_section->channels[channel].mode) {
    case EFFECT_SOUND_MODE_FIRST:
        sound = g_battle_effect_flags_section->channels[channel].sound_ids[0];
        break;
    case EFFECT_SOUND_MODE_ALTERNATE:
        sound = g_battle_effect_flags_section->channels[channel].sound_ids[count & 1];
        break;
    case EFFECT_SOUND_MODE_FIRST_THEN_SECOND:
        if (count == 0) {
            sound = g_battle_effect_flags_section->channels[channel].sound_ids[0];
        } else {
            sound = g_battle_effect_flags_section->channels[channel].sound_ids[1];
        }
        break;
    case EFFECT_SOUND_MODE_FIRST_THEN_ALTERNATE:
        if (count != 0) {
            sound = g_battle_effect_flags_section->channels[channel].sound_ids[1 + (count & 1)];
        } else {
            sound = g_battle_effect_flags_section->channels[channel].sound_ids[0];
        }
        break;
    case EFFECT_SOUND_MODE_CYCLE:
        sound = g_battle_effect_flags_section->channels[channel].sound_ids[count % 3];
        break;
    }
    return sound;
}
