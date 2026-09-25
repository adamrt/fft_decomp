#include "fft/battle.h"
#include "psx/types.h"

void battle_camera_shift_acting_unit_vector(void) {
    battle_camera_calculate_acting_unit_vector_normal();
    g_battle_current_vector.vx *= 2;
    g_battle_current_vector.vy *= 2;
    g_battle_current_vector.vz *= 2;
}
