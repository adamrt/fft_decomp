#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_unit.h"
#include "fft/map.h"
#include "psx/suzuki.h"
#include "psx/types.h"

/*
 * Advances one sound track of the parent timeline by a frame. The 0x1e-byte
 * track holds nine 16-bit durations at 0x00, nine channel bytes at 0x12
 * (0 and 1 none, otherwise a sound channel of the flags section + 2) and its
 * entry count at 0x1c; entering an entry plays the channel's next sound.
 * Sound ids are relative to g_effect_sound_data_base, the effect sound
 * section's id set by effect_load_sound_section.
 */
void effect_advance_parent_sound_track(
    effect_sound_track_t* schedule, s16* entry_index, s16* countdown, u8* unused_sound_state) {
    s16 index = *entry_index;
    s32 effect_kind;
    s32 sound_index;

    if (index < schedule->entry_count) {
        if (*countdown == 0) {
            *entry_index = index + 1;
            *countdown = schedule->durations[index];
            effect_kind = schedule->channels[index];

            if (effect_kind >= 2) {
                sound_index = effect_select_sound_id(effect_kind - 2);

                if (sound_index != 0) {
                    suzuki_sound_play_sfx(g_effect_sound_data_base + sound_index);
                }
            }
        }
        *countdown -= 1;
    }
}
