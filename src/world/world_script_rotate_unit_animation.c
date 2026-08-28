#include "fft/battle.h"
#include "fft/event.h"
#include "fft/world.h"
#include "psx/types.h"

void world_script_rotate_unit_animation(const u8* parameters) {
    const u8* data = parameters;
    u8 rotation;
    s16 value;
    s16 call_misc_id;
    s16 misc_id;
    s32 index;
    world_unit_animation_state_t* state;

    misc_id = world_script_load_halfword(data);
    rotation = data[2];
    value = world_script_load_halfword(data + 3);
    call_misc_id = world_get_misc_id(misc_id);
    misc_id = call_misc_id;
    if (data[4] == 0) {
        battle_unit_clear_facing_update_suppression(call_misc_id);
    } else {
        battle_unit_set_facing_update_suppression(call_misc_id);
    }
    index = misc_id;
    state = &g_world_unit_animation_states[index];
    state->target_facing = rotation;
    state->delay = 0;
    state->rotating = 0;
    world_unit_set_facing(index, rotation);
    world_unit_set_animation(index, value);
}
