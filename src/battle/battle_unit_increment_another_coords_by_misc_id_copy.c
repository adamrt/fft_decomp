#include "fft/battle.h"
#include "psx/types.h"

/* The target contains two separately addressable copies of this accumulator. */
s32 battle_unit_increment_another_coords_by_misc_id_copy(u32 misc_id, const battle_screen_coords_t* amount) {
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
