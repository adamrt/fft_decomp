#include "fft/unit_slots.h"
#include "fft/world.h"

void world_unit_clear_status_staging_data(void) {
    s32 unit_index;

    unit_index = 0;
    do {
        g_world_unit_status_staging_data->state[unit_index] = 0;
        g_world_unit_status_staging_data->exit_mode[unit_index] = 0;
        unit_index++;
    } while (unit_index < BATTLE_UNIT_SLOT_COUNT);
}
