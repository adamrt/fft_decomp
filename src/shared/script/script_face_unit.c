#include "fft/battle_script.h"
#include "fft/battle_unit.h"
#include "fft/script.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* FACE_UNIT and FACE_EACH_OTHER event instructions:
 * turn two units toward each other.
 *
 * Parameters: first 16-bit unit id, second 16-bit unit id, rotate mode, speed index
 * into g_unit_rotation_speed_frames, and a delay multiplier. Every unit record of the second unit turns
 * toward the first; unless second_only is set (FaceUnit), the first unit also turns
 * toward the second. Successive second-unit records are staggered like FaceTile.
 * The angle is built in place (negate, then add) so combine cannot fold the two
 * constant additions into one. */
void script_face_unit(const event_face_unit_parameters_t* parameters, s32 second_only) {
    SVECTOR position;
    SVECTOR target;
    u16 unit_id;
    s32 filter_mode;
    u16 runtime_id;
    s32 i;
    s32 order;
    s32 facing;
    s32 reverse;
    s32 dx;
    s32 dy;
    unit_animation_state_t* state;

    order = 1;
    runtime_id = script_read_s16(parameters->first_unit_id_le);
    unit_id = script_read_s16(parameters->second_unit_id_le);
    if (script_prepare_unit_filter(&unit_id, &filter_mode) != 0) {
        runtime_id = script_resolve_runtime_id(runtime_id);
        if (runtime_id != EVENT_RUNTIME_ID_NONE) {
            for (i = 0; i < EVENT_UNIT_SLOT_COUNT; i++) {
                if (script_filter_unit_id_by_mode(&unit_id, &g_script_unit_face_slot_order[i], &filter_mode) != 0
                    && runtime_id != unit_id) {
                    unit_copy_world_coords(runtime_id, &position);
                    unit_copy_world_coords(unit_id, &target);
                    dx = target.vx - position.vx;
                    dy = target.vz - position.vz;
                    facing = -ratan2(dy, dx);
                    facing += 0xC00;
                    reverse = facing + 0x800;
                    facing &= 0xF00;
                    reverse &= 0xF00;
                    facing >>= 8;
                    reverse >>= 8;
                    if (second_only == 0) {
                        state = &g_unit_animation_states[runtime_id];
                        state->target_facing = facing;
                        state->rotate_mode = parameters->mode;
                        state->frames_per_step = g_unit_rotation_speed_frames[parameters->speed];
                        state->step_counter = 0;
                        state->rotating = 1;
                        state->delay = parameters->delay >> 2;
                    }
                    state = &g_unit_animation_states[unit_id];
                    state->target_facing = reverse;
                    state->rotate_mode = parameters->mode;
                    state->frames_per_step = g_unit_rotation_speed_frames[parameters->speed];
                    state->step_counter = 0;
                    state->rotating = 1;
                    state->delay = parameters->delay * order++ / 4;
                    if (filter_mode == 0) {
                        break;
                    }
                }
            }
        }
    }
}
