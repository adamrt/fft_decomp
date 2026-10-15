#include "fft/battle_unit.h"

/* The runtime (display) record whose runtime_id matches the requested ID, or 0. */
battle_unit_t* battle_unit_get_by_runtime_id(u16 runtime_id) {
    battle_unit_t* unit;

    unit = g_battle_unit_list_head;
    while (unit != 0) {
        if (unit->runtime_id == runtime_id) {
            return unit;
        }
        unit = unit->previous;
    }
    return 0;
}
