#include "fft/event.h"
#include "fft/world.h"
#include "psx/types.h"

extern s32 battle_unit_get_facing_nibble_by_misc_id(u32 misc_id);

/* RotateUnit event instruction: turn the addressed unit(s) to a facing.
 *
 * Parameters: 0x00 unit id (halfword), 0x02 facing code, 0x03 rotate mode,
 * 0x04 rotation speed (index into g_world_rotation_speed_frames), 0x05 delay multiplier.
 * Facing codes 0x11-0x13 are relative to the unit's current facing, 0x10 is
 * relative to the camera quadrant, 0x14 restores the saved facing, and any
 * other value is absolute. Matching records are staggered by delay * n / 4.
 *
 * The restore arm carries its own copy of the misc_index test, which is what
 * retail does: it reloads misc_index and jumps past the absolute arm's own
 * reload, so cross-jumping merges only the branch and the loop increment. */
void world_rotate_unit(const u8* parameters) {
    u16 unit_id;
    s32 misc_index;
    s32 i;
    s32 order;
    s32 facing;
    s32 remaining;
    world_unit_animation_state_t* state;

    order = 1;
    unit_id = world_script_load_halfword(parameters);
    if (world_unit_try_get_misc_data_by_id(&unit_id, &misc_index) != 0) {
        for (i = 0; i < 21; i++) {
            if (world_script_filter_unit_id_by_mode(&unit_id, (u16*)&g_world_unit_face_slot_order[i], &misc_index)
                != 0) {
                facing = parameters[2];
                if ((u32)(facing - 0x11) < 3) {
                    facing = facing & 0xF;
                    /* The definition's u32 parameter and s16 return conversions would change this call's codegen. */
                    facing = ((s32 (*)(s32))battle_unit_get_facing_nibble_by_misc_id)(unit_id) + facing * 4;
                    facing = facing & 0xF;
                } else if (facing == 0x10) {
                    facing = g_world_rotation_speed_frames[4 + ((world_camera_wrap_yaw_angle() & 0xC00) >> 10)] * 4;
                } else if (facing == 0x14) {
                    state = &g_world_unit_animation_states[unit_id];
                    if (state->initial_facing != 0xFF) {
                        state->rotate_mode = 0;
                        state->target_facing = state->initial_facing;
                        state->frames_per_step = g_world_rotation_speed_frames[parameters[4]];
                        state->step_counter = 0;
                        state->rotating = 1;
                        state->delay = parameters[5] * order++ / 4;
                    }
                    remaining = misc_index;
                    /* Stops cross-jumping from merging this arm's setup and test
                     * into the absolute arm's. */
                    __asm__("" : "=r"(remaining) : "0"(remaining));
                    if (remaining == 0) {
                        return;
                    }
                    continue;
                }
                state = &g_world_unit_animation_states[unit_id];
                state->target_facing = facing;
                state->rotate_mode = parameters[3];
                state->frames_per_step = g_world_rotation_speed_frames[parameters[4]];
                state->step_counter = 0;
                state->rotating = 1;
                state->delay = parameters[5] * order++ / 4;
                if (misc_index == 0) {
                    return;
                }
            }
        }
    }
}
