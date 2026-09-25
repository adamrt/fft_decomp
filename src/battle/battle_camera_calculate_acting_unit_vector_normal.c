#include "fft/battle.h"
#include "psx/types.h"

void battle_camera_calculate_acting_unit_vector_normal(void) {
    VECTOR delta;
    battle_unit_misc_data_t* unit;

    unit = battle_unit_get_source_misc_data();
    delta.vx = unit->screen.vx - g_battle_camera_current_real_coords.vx / ONE;
    delta.vy = unit->screen.vy - g_battle_camera_current_real_coords.vy / ONE;
    delta.vz = unit->screen.vz - g_battle_camera_current_real_coords.vz / ONE;
    VectorNormal(&delta, &g_battle_current_vector);
    g_battle_current_vector.vy = g_battle_current_vector.vy << 2;
}
