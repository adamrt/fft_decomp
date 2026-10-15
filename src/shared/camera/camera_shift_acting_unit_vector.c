#include "fft/battle_action.h"
#include "fft/battle_camera.h"

/* The acting-unit camera step vector at double length, used when the unit is
 * outside the wider screen window. */
void camera_shift_acting_unit_vector(void) {
    camera_calculate_acting_unit_vector_normal();
    g_camera_step_vector.vx *= 2;
    g_camera_step_vector.vy *= 2;
    g_camera_step_vector.vz *= 2;
}
