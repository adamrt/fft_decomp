#include "fft/effect.h"

/* Advance the caster palette track one frame.
 *
 * When a keyframe with preset bit 7 starts, its colour is applied to the
 * caster's unit palette. */
void battle_effect_advance_caster_palette_track(
    battle_effect_palette_track_t* track, s16* keyframe, s16* remaining_frames, s32 caster_misc_id) {
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
            if (preset & 0x80) {
                red = track->color[index][0];
                green = track->color[index][1];
                blue = track->color[index][2];
                battle_gfx_start_misc_unit_palette_modulation(
                    preset & 0x7f, track->duration[index], caster_misc_id, red, green, blue);
            }
        }
        *remaining_frames = *remaining_frames - 1;
    }
}
