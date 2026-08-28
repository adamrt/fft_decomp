#include "fft/battle.h"
#include "fft/battle_ability.h"
#include "psx/types.h"

void battle_status_remove(void) {
    s32 i;
    if (g_current_ability.status_infliction.type & BATTLE_STATUS_INFLICTION_TYPE_CANCEL) {
        i = 0;
        do {
            g_current_ability_canceled_statuses[i] = g_current_ability.status_infliction.statuses[i];
            i++;
        } while (i < BATTLE_STATUS_BYTE_COUNT);
    }
}
