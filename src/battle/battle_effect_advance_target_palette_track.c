#include "fft/battle.h"
#include "psx/types.h"

/* Advance the target palette track one frame.
 *
 * The target-side counterpart of battle_effect_advance_caster_palette_track: when a
 * keyframe with preset bit 7 starts, its colour is applied to the field, to
 * every effect target's unit palette and, unless weather flag 2 is set, to the
 * scaled map palette. The loop counter reuses the frame-count variable, which
 * keeps both in $s0 as in the target. */
void battle_effect_advance_target_palette_track(
    battle_effect_palette_track_t* track, s16* keyframe, s16* remaining_frames) {
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
            if (preset & 0x80) {
                red = track->color[index][0];
                green = track->color[index][1];
                blue = track->color[index][2];
                battle_map_color_field(preset & 0x7f, track->duration[index], red, green, blue);
                for (i = 0; i < g_battle_effect_coord_data.palette_target_count; i++) {
                    battle_gfx_start_misc_unit_palette_modulation(preset & 0x7f, track->duration[index],
                        g_battle_effect_palette_target_misc_ids[i], red, green, blue);
                }
                if (!(g_battle_map_weather_flags & 2)) {
                    battle_map_start_darkness_blend(
                        preset & 0x7f, track->duration[index], red * 8, green * 8, blue * 8);
                }
            }
        }
        *remaining_frames = *remaining_frames - 1;
    }
}
