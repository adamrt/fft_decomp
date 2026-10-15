#include "fft/battle_unit.h"

/* The unit's drawn position: its integer world position plus the event and SEQ offsets,
 * with the Float bob added to the height. */
void battle_unit_copy_draw_coords(SVECTOR* destination, const battle_unit_t* unit) {
    /* float_bob_offset is read signed (lh) here. */
    main_util_set_svector(destination, unit->world_coords.vx + unit->event_offset.vx + unit->seq_offset.vx,
        unit->world_coords.vy + unit->event_offset.vy + unit->seq_offset.vy + (s16)unit->float_bob_offset,
        unit->world_coords.vz + unit->event_offset.vz + unit->seq_offset.vz);
}
