#include "fft/battle_script.h"
#include "fft/battle_unit.h"
#include "fft/script.h"
#include "psx/types.h"

/*
 * Event instruction 0x69 (FaceTile): turn the addressed unit(s) to face a tile.
 *
 * Parameters: 0x00 unit id (16-bit value), 0x02 tile x, 0x03 tile y, 0x04 unused,
 * 0x05 clockwise flag, 0x06 rotation speed (index into g_unit_rotation_speed_frames), 0x07
 * rotation time. Each matching unit's screen position is compared with the
 * tile center to pick one of 16 facings; the n-th matching unit gets
 * rotation time * n / 4.
 */
void script_face_tile(const event_face_tile_parameters_t* parameters) {
    SVECTOR screen;
    SVECTOR tile;
    u16 unit_id;
    s32 state;
    s32 i;
    unit_animation_state_t* rotation_state;
    s32 order;
    s32 facing;
    s32 dx;
    s32 dy;

    order = 1;
    tile.vx = parameters->x * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
    tile.vz = parameters->y * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
    unit_id = script_read_s16(parameters->unit_id_le);
    if (script_prepare_unit_filter(&unit_id, &state) == 0) {
        return;
    }
    for (i = 0; i < EVENT_UNIT_SLOT_COUNT; i++) {
        if (script_filter_unit_id_by_mode(&unit_id, &g_script_unit_face_slot_order[i], &state) != 0) {
            unit_copy_world_coords(unit_id, &screen);
            dx = tile.vx - screen.vx;
            dy = tile.vz - screen.vz;
            facing = ((-ratan2(dy, dx) + 0xC00) & 0xF00) >> 8;
            rotation_state = &g_unit_animation_states[unit_id];
            rotation_state->target_facing = facing;
            rotation_state->rotate_mode = parameters->mode;
            rotation_state->frames_per_step = g_unit_rotation_speed_frames[parameters->speed];
            rotation_state->step_counter = 0;
            rotation_state->rotating = 1;
            rotation_state->delay = (parameters->delay * order++) / 4;
            if (state == 0) {
                return;
            }
        }
    }
}
