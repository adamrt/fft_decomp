#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "psx/libgte.h"
#include "psx/types.h"

void battle_unit_copy_draw_coords(SVECTOR* destination, const battle_unit_t* unit) {
    /* float_bob_offset is read signed (lh) here. */
    main_util_set_svector(destination, unit->world_coords.vx + unit->event_offset.vx + unit->seq_offset.vx,
        unit->world_coords.vy + unit->event_offset.vy + unit->seq_offset.vy + (s16)unit->float_bob_offset,
        unit->world_coords.vz + unit->event_offset.vz + unit->seq_offset.vz);
}
