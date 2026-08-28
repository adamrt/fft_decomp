#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/event.h"
#include "fft/world.h"
#include "psx/types.h"

extern s32 battle_unit_get_facing_nibble_by_misc_id(u32 misc_id);
/* 7 bytes per misc unit, shared layout with the WORLD rotation table. */
extern world_unit_animation_state_t g_battle_unit_misc_rotation_data[];

/* RotateUnit event instruction: turn the addressed unit(s) to a facing.
 *
 * Parameters: 0x00 unit id (halfword), 0x02 facing code, 0x03 rotate mode,
 * 0x04 rotation speed (index into g_battle_rotation_speed_frames), 0x05 delay multiplier.
 * Facing codes 0x11-0x13 are relative to the unit's current facing, 0x10 is
 * relative to the camera quadrant, 0x14 restores the saved facing, and any
 * other value is absolute. Matching records are staggered by delay * n / 4.
 *
 * The restore arm carries its own copy of the misc_index test, which is what
 * retail does: it reloads misc_index and jumps past the absolute arm's own
 * reload, so cross-jumping merges only the branch and the loop increment. */
void battle_rotate_unit(const u8* parameters) {
    u16 unit_id;
    s32 misc_index;
    s32 i;
    s32 order;
    s32 facing;
    s32 remaining;
    s32 restore_misc_index;
    world_unit_animation_state_t* state;

    order = 1;
    unit_id = battle_script_load_halfword(parameters);
    if (battle_unit_try_get_misc_data_by_unit_id(&unit_id, &misc_index) != 0) {
        for (i = 0; i < EVENT_UNIT_SLOT_COUNT; i++) {
            if (battle_script_filter_unit_id_by_mode(&unit_id, &g_battle_script_unit_filter_modes[i], &misc_index)
                != 0) {
                facing = parameters[2];
                if ((u32)(facing - 0x11) < 3) {
                    facing = facing & 0xF;
                    facing = battle_unit_get_facing_nibble_by_misc_id(unit_id) + facing * 4;
                    facing = facing & 0xF;
                } else if (facing == 0x10) {
                    facing = g_battle_rotation_speed_frames[4 + ((battle_camera_wrap_yaw_angle() & 0xC00) >> 10)] * 4;
                } else if (facing == 0x14) {
                    state = &g_battle_unit_misc_rotation_data[unit_id];
                    if (state->initial_facing != 0xFF) {
                        state->rotate_mode = 0;
                        state->target_facing = state->initial_facing;
                        state->frames_per_step = g_battle_rotation_speed_frames[parameters[4]];
                        state->step_counter = 0;
                        state->rotating = 1;
                        state->delay = parameters[5] * order++ / 4;
                        /* Keep the restore arm's stores separate from the absolute arm. */
                        __asm__ volatile("");
                    }
                    restore_misc_index = *(volatile s32*)&misc_index;
                    /* Keep the restore arm's stack reload before its loop exit. */
                    __asm__("" : "=r"(restore_misc_index) : "0"(restore_misc_index));
                    if (restore_misc_index == 0) {
                        return;
                    }
                    continue;
                }
                state = &g_battle_unit_misc_rotation_data[unit_id];
                state->target_facing = facing;
                state->rotate_mode = parameters[3];
                state->frames_per_step = g_battle_rotation_speed_frames[parameters[4]];
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
