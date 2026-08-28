#include "fft/battle.h"

s32 battle_map_start_texture_animation(s32 frame_duration, s32 polygon_group, s32 first_polygon, s32 last_polygon) {
    s32 index;
    s32 active;
    s32 result;
    map_texture_animation_state_t* state;

    index = 0;
    active = 1;
    state = g_battle_map_texture_animation_states;

    /* Goto loop: for/while/do forms engage the loop optimizer, which splits
     * the slot pointer into extra induction variables the target lacks. */
loop:
    if (state->active != 0) {
        goto occupied;
    }
    result = index + 1;
    state->active = active;
    state->polygon_group = polygon_group;
    state->first_polygon = first_polygon;
    state->last_polygon = last_polygon;
    state->frame_duration = frame_duration;
    return result;

occupied:
    index++;
    if (index < MAP_TEXTURE_ANIMATION_CAPACITY) {
        state++;
        goto loop;
    }
    return 0;
}
