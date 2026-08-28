#include "fft/battle.h"
#include "fft/unit_slots.h"

void battle_unit_clear_status_staging_data(void) {
    s32 i;
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        g_battle_unit_status_staging_data->state[i] = 0;
        g_battle_unit_status_staging_data->exit_mode[i] = 0;
    }
}
