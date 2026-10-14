#include "fft/battle_unit.h"
#include "fft/script.h"
#include "psx/types.h"

/* Set facing and animation immediately; the animation high byte also controls facing-update suppression. */
void script_unit_animation_rotate_event_instruction(const event_unit_anim_rotate_parameters_t* parameters) {
    u8 rotation;
    s16 animation;
    s16 call_runtime_id;
    s16 runtime_id;
    s32 index;
    unit_animation_state_t* rotation_state;

    runtime_id = script_read_s16(parameters->unit_id_le);
    rotation = parameters->rotation;
    animation = script_read_s16(parameters->animation_le);
    call_runtime_id = script_resolve_runtime_id(runtime_id);
    runtime_id = call_runtime_id;
    if (parameters->animation_le[1] == 0) {
        battle_unit_clear_facing_update_suppression(call_runtime_id);
    } else {
        battle_unit_set_facing_update_suppression(call_runtime_id);
    }
    index = runtime_id;
    rotation_state = &g_unit_animation_states[index];
    rotation_state->target_facing = rotation;
    rotation_state->delay = 0;
    rotation_state->rotating = 0;
    unit_set_facing(index, rotation);
    unit_set_animation(index, animation);
}
