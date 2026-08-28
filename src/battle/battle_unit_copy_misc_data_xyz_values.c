#include "fft/battle.h"
#include "fft/main_gfx.h"

void battle_unit_copy_misc_data_xyz_values(SVECTOR* destination, const battle_unit_misc_data_t* misc_data) {
    /* mounted_height_offset is read signed (lh) here. */
    main_util_set_svector(destination,
        misc_data->screen.vx + misc_data->effect_vector_2.vx + misc_data->effect_vector.vx,
        misc_data->screen.vy + misc_data->effect_vector_2.vy + misc_data->effect_vector.vy
            + *(s16*)&misc_data->mounted_height_offset,
        misc_data->screen.vz + misc_data->effect_vector_2.vz + misc_data->effect_vector.vz);
}
