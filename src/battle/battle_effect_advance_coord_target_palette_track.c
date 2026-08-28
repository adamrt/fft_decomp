#include "fft/battle_effect.h"
#include "fft/effect.h"
#include "psx/types.h"

/* Advance the effect-target palette track one frame over the coord-data
 * target list.
 *
 * The counterpart of battle_effect_advance_target_palette_track that walks the
 * ten-byte records at 0x801bad10 and recolours every unit target. The frame
 * timer and the loop counter are separate locals here: the target keeps the
 * timer in $v0 and the counter in $s1. */
void battle_effect_advance_coord_target_palette_track(
    battle_effect_palette_track_t* track, s16* keyframe, s16* remaining_frames) {
    s16 current;
    s32 index;
    s32 i;
    s32 timer;
    s32 preset;
    s16 red;
    s16 green;
    s16 blue;

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
                for (i = 0; i < g_battle_effect_coord_data.hit_counter; i++) {
                    if (g_battle_effect_targets[i].target_type == 0) {
                        battle_gfx_start_misc_unit_palette_modulation(preset & 0x7f, track->duration[index],
                            g_battle_effect_targets[i].id.misc_id, red, green, blue);
                    }
                }
            }
        }
        *remaining_frames = *remaining_frames - 1;
    }
}
