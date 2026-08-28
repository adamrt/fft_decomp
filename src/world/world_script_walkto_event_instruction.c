#include "fft/event.h"

/* Start event opcode 0x28 movement for one unit.
 *
 * WORLD dispatches this directly; BATTLE uses the main-stack callback bridge. */
void world_script_walkto_event_instruction(const event_walk_to_parameters_t* parameters) {
    s16 coords[3];
    s32 misc_id;

    misc_id = world_script_load_halfword(parameters->unit_id_le);
    misc_id = world_get_misc_id((s16)misc_id);
    if (misc_id != EVENT_MISC_ID_NONE) {
        coords[0] = parameters->x;
        coords[2] = parameters->y;
        coords[1] = parameters->elevation;
        battle_move_start_unit_walk_to(misc_id, coords, 0x100 - (parameters->elevation_flag << 8), 3,
            (s16)world_script_load_halfword(parameters->speed_le));
    }
}
