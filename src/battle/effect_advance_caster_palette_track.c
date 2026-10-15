#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_unit.h"
#include "fft/map.h"
#include "psx/suzuki.h"
#include "psx/types.h"

/* Advance the caster color track one frame: an APPLY keyframe recolors the
 * caster. */
void effect_advance_caster_palette_track(
    effect_palette_track_t* track, s16* keyframe, s16* remaining_frames, s32 caster_runtime_id) {
    s16 current;
    s32 index;
    s32 timer;
    s32 preset;
    s8 red;
    s8 green;
    s8 blue;

    current = *keyframe;
    if (current < track->count - 1) {
        if (*remaining_frames == 0) {
            *keyframe = current + 1;
            index = current;
            timer = track->duration[index];
            if (timer != 0) {
                timer *= 8;
            } else {
                timer = 1;
            }
            *remaining_frames = timer;
            preset = track->preset[index];
            if (preset & EFFECT_COLOR_KEYFRAME_APPLY) {
                red = track->color[index].r;
                green = track->color[index].g;
                blue = track->color[index].b;
                battle_gfx_apply_unit_palette_modulation_to_units(preset & EFFECT_COLOR_KEYFRAME_MODE_MASK,
                    track->duration[index], caster_runtime_id, red, green, blue);
            }
        }
        *remaining_frames = *remaining_frames - 1;
    }
}
