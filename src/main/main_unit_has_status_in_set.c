#include "fft/main_runtime.h"
#include "fft/main_unit.h"

s32 main_unit_has_status_in_set(const battle_stats_t* unit, main_status_check_set_e status_set_index) {
    const u8* status_set = g_main_status_check_sets[0];
    s32 index = 0;

    status_set += status_set_index * BATTLE_STATUS_BYTE_COUNT;

    do {
        if ((unit->status_sets.current[index] & status_set[index]) != 0) {
            return 1;
        }
        index++;
    } while (index < BATTLE_STATUS_BYTE_COUNT);
    return 0;
}
