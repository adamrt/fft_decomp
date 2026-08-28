#include "fft/unit_slots.h"
#include "fft/world.h"
#include "psx/types.h"

/* FaceTile event instruction: turn every misc record of a unit toward a map tile.
 *
 * Parameters: unit id halfword, tile x, tile y, unused byte, rotate mode, speed index
 * into g_world_rotation_speed_frames, and a delay multiplier that staggers successive units by a quarter
 * of its value each. The second argument to world_process_unit_misc_state_value is
 * taken from the g_world_unit_face_slot_order halfword table rather than a local counter. */
void world_script_face_tile(const u8* parameters) {
    s16 position[3];
    s16 target[3];
    u16 unit_id;
    s32 misc_index;
    s32 i;
    s32 order;
    s32 facing;
    s32 dx;
    s32 dy;
    world_unit_animation_state_t* state;

    order = 1;
    target[0] = parameters[2] * 28 + 14;
    target[2] = parameters[3] * 28 + 14;
    unit_id = world_script_load_halfword(parameters);
    if (world_unit_try_get_misc_data_by_id(&unit_id, &misc_index) != 0) {
        for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
            if (world_script_filter_unit_id_by_mode(&unit_id, (u16*)&g_world_unit_face_slot_order[i], &misc_index)
                != 0) {
                world_unit_copy_screen_data_by_misc_id(unit_id, position);
                dx = target[0] - position[0];
                dy = target[2] - position[2];
                facing = ((-ratan2(dy, dx) + 0xC00) & 0xF00) >> 8;
                state = &g_world_unit_animation_states[unit_id];
                state->target_facing = facing;
                state->rotate_mode = parameters[5];
                state->frames_per_step = g_world_rotation_speed_frames[parameters[6]];
                state->step_counter = 0;
                state->rotating = 1;
                state->delay = parameters[7] * order++ / 4;
                if (misc_index == 0) {
                    break;
                }
            }
        }
    }
}
