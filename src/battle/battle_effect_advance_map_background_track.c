#include "fft/battle.h"
#include "psx/types.h"

/* Advance the map background track one frame.
 *
 * A starting keyframe with preset bit 7 modulates the background gradient by
 * its doubled colour (and the scaled palette when weather flag 2 is set);
 * otherwise both colours become the new gradient stops, with the first also
 * driving the darkness colour under weather flag 2. */
void battle_effect_advance_map_background_track(
    battle_effect_background_track_t* track, s16* keyframe, s16* remaining_frames) {
    map_background_gradient_colors_t colors;
    s16 current;
    s32 index;
    s16 duration;
    u8 preset;
    s32 mode;
    s32 red;
    s32 green;
    s32 blue;

    current = *keyframe;
    if (current < track->count - 1) {
        if (*remaining_frames == 0) {
            *keyframe = current + 1;
            index = current;
            duration = track->duration[index];
            if (duration != 0) {
                duration *= 8;
            } else {
                duration = 1;
            }
            *remaining_frames = duration;
            preset = track->preset[index];
            if (preset & 0x80) {
                mode = preset & 0x7f;
                red = track->color[index][0] * 2;
                green = track->color[index][1] * 2;
                blue = track->color[index][2] * 2;
                battle_map_modify_background_gradient(mode, track->duration[index], red, green, blue);
                if (g_battle_map_weather_flags & 2) {
                    battle_map_start_darkness_blend(mode, track->duration[index], red, green, blue);
                }
            } else {
                colors.first.red = track->color[index][0];
                colors.first.green = track->color[index][1];
                colors.first.blue = track->color[index][2];
                colors.second.red = track->second_color[index][0];
                colors.second.green = track->second_color[index][1];
                colors.second.blue = track->second_color[index][2];
                battle_map_set_background_gradient(track->duration[index], &colors);
                if (g_battle_map_weather_flags & 2) {
                    battle_map_set_darkness_color(track->duration[index], &colors.first);
                }
            }
        }
        *remaining_frames = *remaining_frames - 1;
    }
}
