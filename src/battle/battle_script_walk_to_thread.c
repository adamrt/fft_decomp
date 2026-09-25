#include "fft/battle.h"

/* Start event opcode 0x28 movement for one unit.
 *
 * Coordinates are ordered x, elevation, y for the movement subsystem. The
 * final payload bit selects absolute elevation or a one-level addend. */
void battle_script_walk_to_thread(const event_walk_to_parameters_t* parameters) {
    const event_walk_to_parameters_t* data = parameters;
    /* Pin: unpinned, GCC swaps $s0 and $s1 between `misc_id` and `data`. */
    register s32 misc_id __asm__("$16");
    s16 coordinates[3];

    misc_id = battle_get_misc_id((s16)battle_script_load_halfword(data->unit_id_le));
    if (misc_id != EVENT_MISC_ID_NONE) {
        coordinates[0] = data->x;
        coordinates[2] = data->y;
        coordinates[1] = data->elevation;
        battle_move_start_unit_walk_to(misc_id, coordinates, 0x100 - (data->elevation_flag << 8), 3,
            (s16)battle_script_load_halfword(data->speed_le));
    }
}
