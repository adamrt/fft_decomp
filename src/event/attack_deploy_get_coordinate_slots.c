#include "fft/attack.h"
#include "psx/types.h"

void* attack_deploy_get_coordinate_slots(void) {
    void* coordinate_slots;

    if (g_attack_deploy_second_pass != 0) {
        coordinate_slots = &g_attack_deployed_unit_map_coordinates[6];
    } else {
        g_attack_deploy_second_pass = 1;
        coordinate_slots = g_attack_deployed_unit_map_coordinates;
    }

    return coordinate_slots;
}
