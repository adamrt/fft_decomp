#include "fft/event_attack.h"

/* Return the deployed-unit coordinate slots to fill: the first six on the
 * first call, the slots from index 6 on every later call. */
unit_deployment_coordinates_t* attack_deploy_get_coordinate_slots(void) {
    unit_deployment_coordinates_t* coordinate_slots;

    if (g_attack_deploy_second_pass != 0) {
        coordinate_slots = &g_attack_deployed_unit_map_coordinates[6];
    } else {
        g_attack_deploy_second_pass = 1;
        coordinate_slots = g_attack_deployed_unit_map_coordinates;
    }

    return coordinate_slots;
}
