#include "fft/event.h"
#include "fft/unit_slots.h"
#include "fft/world.h"
#include "psx/types.h"

/* FaceUnit and FaceUnit2 event instructions: turn two units toward each other.
 *
 * Parameters: first unit id halfword, second unit id halfword, rotate mode, speed index
 * into g_world_rotation_speed_frames, and a delay multiplier. Every misc record of the second unit turns
 * toward the first; unless second_only is set (FaceUnit), the first unit also turns
 * toward the second. Successive second-unit records are staggered like FaceTile.
 * The angle is built in place (negate, then add) so combine cannot fold the two
 * constant additions into one. */
void world_script_face_unit(const u8* parameters, s32 second_only) {
    s16 position[4];
    s16 target[4];
    u16 unit_id;
    s32 misc_index;
    u16 misc_id;
    s32 i;
    s32 order;
    s32 facing;
    s32 reverse;
    s32 dx;
    s32 dy;
    world_unit_animation_state_t* state;

    order = 1;
    misc_id = world_script_load_halfword(parameters);
    unit_id = world_script_load_halfword(parameters + 2);
    if (world_unit_try_get_misc_data_by_id(&unit_id, &misc_index) != 0) {
        misc_id = world_get_misc_id(misc_id);
        if (misc_id != EVENT_MISC_ID_NONE) {
            for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
                if (world_script_filter_unit_id_by_mode(&unit_id, (u16*)&g_world_unit_face_slot_order[i], &misc_index)
                        != 0
                    && misc_id != unit_id) {
                    world_unit_copy_screen_data_by_misc_id(misc_id, position);
                    world_unit_copy_screen_data_by_misc_id(unit_id, target);
                    dx = target[0] - position[0];
                    dy = target[2] - position[2];
                    facing = -ratan2(dy, dx);
                    facing += 0xC00;
                    reverse = facing + 0x800;
                    facing &= 0xF00;
                    reverse &= 0xF00;
                    facing >>= 8;
                    reverse >>= 8;
                    if (second_only == 0) {
                        state = &g_world_unit_animation_states[misc_id];
                        state->target_facing = facing;
                        state->rotate_mode = parameters[4];
                        state->frames_per_step = g_world_rotation_speed_frames[parameters[5]];
                        state->step_counter = 0;
                        state->rotating = 1;
                        state->delay = parameters[6] >> 2;
                    }
                    state = &g_world_unit_animation_states[unit_id];
                    state->target_facing = reverse;
                    state->rotate_mode = parameters[4];
                    state->frames_per_step = g_world_rotation_speed_frames[parameters[5]];
                    state->step_counter = 0;
                    state->rotating = 1;
                    state->delay = parameters[6] * order++ / 4;
                    if (misc_index == 0) {
                        break;
                    }
                }
            }
        }
    }
}
