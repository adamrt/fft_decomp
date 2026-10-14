#include "fft/battle_effect.h"
#include "fft/battle_map.h"
#include "fft/map.h"
#include "psx/types.h"

/* Advance the screen color track one frame.
 *
 * A starting APPLY keyframe modulates the background gradient by
 * its doubled color (and the scaled palette when weather flag 2 is set);
 * otherwise both colors become the new gradient stops, with the first also
 * driving the darkness color under weather flag 2. */
void effect_advance_map_background_track(effect_background_track_t* track, s16* keyframe, s16* remaining_frames) {
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
            if (preset & EFFECT_COLOR_KEYFRAME_APPLY) {
                mode = preset & EFFECT_COLOR_KEYFRAME_MODE_MASK;
                red = track->color[index].r * 2;
                green = track->color[index].g * 2;
                blue = track->color[index].b * 2;
                map_modify_background_gradient(mode, track->duration[index], red, green, blue);
                if (g_map_weather_flags & MAP_WEATHER_FLAG_IGNORE_WEATHER) {
                    map_start_darkness_blend(mode, track->duration[index], red, green, blue);
                }
            } else {
                colors.colors[MAP_BACKGROUND_GRADIENT_TOP].r = track->color[index].r;
                colors.colors[MAP_BACKGROUND_GRADIENT_TOP].g = track->color[index].g;
                colors.colors[MAP_BACKGROUND_GRADIENT_TOP].b = track->color[index].b;
                colors.colors[MAP_BACKGROUND_GRADIENT_BOTTOM].r = track->second_color[index].r;
                colors.colors[MAP_BACKGROUND_GRADIENT_BOTTOM].g = track->second_color[index].g;
                colors.colors[MAP_BACKGROUND_GRADIENT_BOTTOM].b = track->second_color[index].b;
                map_set_background_gradient(track->duration[index], &colors);
                if (g_map_weather_flags & MAP_WEATHER_FLAG_IGNORE_WEATHER) {
                    map_set_darkness_color(track->duration[index], &colors.colors[MAP_BACKGROUND_GRADIENT_TOP]);
                }
            }
        }
        *remaining_frames = *remaining_frames - 1;
    }
}
