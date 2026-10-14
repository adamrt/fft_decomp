#include "fft/battle_effect.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "psx/types.h"

/* Advance the surroundings color track one frame.
 *
 * When an APPLY keyframe starts, its color is applied to the field, to the
 * palette of every spectator unit (those that are neither a target nor the
 * caster) and, unless weather flag 2 is set, to the scaled map palette. The
 * loop counter reuses the frame-count variable, which keeps both in $s0 as in
 * the target. */
void effect_advance_target_palette_track(effect_palette_track_t* track, s16* keyframe, s16* remaining_frames) {
    s16 current;
    s32 index;
    s32 i;
    s32 preset;
    s16 red;
    s16 green;
    s16 blue;

    current = *keyframe;
    if (current < track->count - 1) {
        if (*remaining_frames == 0) {
            *keyframe = current + 1;
            index = current;
            i = track->duration[index];
            if (i != 0) {
                i *= 8;
            } else {
                i = 1;
            }
            *remaining_frames = i;
            preset = track->preset[index];
            if (preset & EFFECT_COLOR_KEYFRAME_APPLY) {
                red = track->color[index].r;
                green = track->color[index].g;
                blue = track->color[index].b;
                map_color_field(preset & EFFECT_COLOR_KEYFRAME_MODE_MASK, track->duration[index], red, green, blue);
                for (i = 0; i < g_effect_context.spectator_count; i++) {
                    battle_gfx_apply_unit_palette_modulation_to_units(preset & EFFECT_COLOR_KEYFRAME_MODE_MASK,
                        track->duration[index], g_effect_palette_target_runtime_ids[i], red, green, blue);
                }
                if (!(g_map_weather_flags & MAP_WEATHER_FLAG_IGNORE_WEATHER)) {
                    map_start_darkness_blend(
                        preset & EFFECT_COLOR_KEYFRAME_MODE_MASK, track->duration[index], red * 8, green * 8, blue * 8);
                }
            }
        }
        *remaining_frames = *remaining_frames - 1;
    }
}
