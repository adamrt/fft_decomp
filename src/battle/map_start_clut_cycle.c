#include "fft/battle_map.h"

/* Claim a free color rotation slot: the colors first_color..last_color of a
 * CLUT row rotate one place every frame_duration frames. Returns the slot index +
 * 1 as the handle, or 0 when all eight slots are in use. */
s32 map_start_clut_cycle(s32 frame_duration, s32 clut_row, s32 first_color, s32 last_color) {
    s32 index;
    map_clut_cycle_state_t* state;

    for (index = 0; index < MAP_CLUT_CYCLE_SLOT_COUNT; index++) {
        state = &g_map_clut_cycle_states[index];
        if (state->active == 0) {
            state->active = 1;
            state->clut_row = clut_row;
            state->first_color = first_color;
            state->last_color = last_color;
            state->frame_duration = frame_duration;
            return index + 1;
        }
    }
    return 0;
}
