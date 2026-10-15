#include "fft/battle_action.h"
#include "fft/battle_camera.h"

/* Aims the camera step vector from the camera position at the acting unit: the
 * unit-length direction (ONE) with its height component multiplied by four. */
void camera_calculate_acting_unit_vector_normal(void) {
    VECTOR delta;
    battle_unit_t* unit;

    unit = battle_unit_get_source();
    setVector(&delta, unit->world_coords.vx - g_camera_current_real_coords.vx / ONE,
        unit->world_coords.vy - g_camera_current_real_coords.vy / ONE,
        unit->world_coords.vz - g_camera_current_real_coords.vz / ONE);
    VectorNormal(&delta, &g_camera_step_vector);
    g_camera_step_vector.vy = g_camera_step_vector.vy << 2;
}
