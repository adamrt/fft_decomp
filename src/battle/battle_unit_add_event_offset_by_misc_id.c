#include "fft/battle.h"
#include "psx/types.h"

/* This is the second separately addressable copy of the accumulator. */
s32 battle_unit_add_event_offset_by_misc_id(u32 misc_id, const battle_screen_coords_t* amount) {
    battle_unit_misc_data_t* unit;

    unit = battle_unit_get_misc_data_by_misc_id(misc_id & 0xffff);
    if (unit != 0) {
        unit->effect_vector_2.vx = unit->effect_vector_2.vx + amount->x;
        unit->effect_vector_2.vy = unit->effect_vector_2.vy + amount->z;
        unit->effect_vector_2.vz = unit->effect_vector_2.vz + amount->y;
    } else {
        main_system_handle_pointer_exception(12);
        return 0;
    }
    return 1;
}
