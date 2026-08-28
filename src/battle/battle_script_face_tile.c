#include "fft/battle.h"
#include "fft/event.h"
#include "fft/world.h"
#include "psx/types.h"

extern world_unit_animation_state_t g_battle_unit_misc_rotation_data[];

/*
 * Event instruction 0x69 (FaceTile): turn the addressed unit(s) to face a tile.
 *
 * Parameters: 0x00 unit id (halfword), 0x02 tile x, 0x03 tile y, 0x04 unused,
 * 0x05 clockwise flag, 0x06 rotation speed (index into g_battle_rotation_speed_frames), 0x07
 * rotation time. Each matching unit's screen position is compared with the
 * tile centre to pick one of 16 facings; the n-th matching unit gets
 * rotation time * n / 4.
 */
s32 battle_script_face_tile(u8* parameters) {
    s16 screen[3];
    s16 tile[3];
    u16 unit_id;
    s32 state;
    s32 i;
    world_unit_animation_state_t* rotation_state;
    s32 order;
    s32 facing;
    s32 dx;
    s32 dy;

    order = 1;
    tile[0] = parameters[2] * 28 + 14;
    tile[2] = parameters[3] * 28 + 14;
    unit_id = battle_script_load_halfword(parameters);
    if (battle_unit_try_get_misc_data_by_unit_id(&unit_id, &state) == 0) {
        return;
    }
    for (i = 0; i < EVENT_UNIT_SLOT_COUNT; i++) {
        if (battle_script_filter_unit_id_by_mode(&unit_id, &g_battle_script_unit_filter_modes[i], &state) != 0) {
            battle_unit_copy_misc_screen_coords(unit_id, (u16*)screen);
            dx = tile[0] - screen[0];
            dy = tile[2] - screen[2];
            facing = ((-ratan2(dy, dx) + 0xC00) & 0xF00) >> 8;
            rotation_state = &g_battle_unit_misc_rotation_data[unit_id];
            rotation_state->target_facing = facing;
            rotation_state->rotate_mode = parameters[5];
            rotation_state->frames_per_step = g_battle_rotation_speed_frames[parameters[6]];
            rotation_state->step_counter = 0;
            rotation_state->rotating = 1;
            rotation_state->delay = (parameters[7] * order++) / 4;
            if (state == 0) {
                return;
            }
        }
    }
}
