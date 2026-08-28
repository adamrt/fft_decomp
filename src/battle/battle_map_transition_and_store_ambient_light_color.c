#include "fft/map.h"
#include "psx/types.h"

void battle_map_transition_and_store_ambient_light_color(s32 frame_duration, const map_color_t* color) {
    battle_map_transition_ambient_light_color(frame_duration, color);
    g_battle_map_ambient_light_color = *color;
}
