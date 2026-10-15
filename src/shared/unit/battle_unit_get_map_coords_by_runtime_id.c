#include "fft/battle_unit.h"
#include "psx/types.h"

/* Writes a unit's tile as three 16-bit values in the order x, z, y and returns 1. A
 * runtime id without a record raises pointer exception 12 and returns 0. */
s32 battle_unit_get_map_coords_by_runtime_id(u32 runtime_id, SVECTOR* coords) {
    battle_unit_t* unit = g_battle_unit_list_head;

    for (; unit != 0; unit = unit->previous) {
        if (unit->runtime_id == (runtime_id & 0xffff)) {
            coords->vx = unit->map_x;
            coords->vz = unit->map_y;
            coords->vy = unit->map_z;
            return 1;
        }
    }
    main_system_handle_pointer_exception(12);
    return 0;
}
