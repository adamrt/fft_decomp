#include "fft/unit.h"

/* Resets every unit slot's status staging state and inflict-status mode to none. */
void unit_clear_status_staging_data(void) {
    s32 i;
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        g_unit_status_staging_data->state[i] = UNIT_STATUS_STAGING_NONE;
        g_unit_status_staging_data->inflict_status_mode[i] = UNIT_INFLICT_STATUS_MODE_NONE;
    }
}
