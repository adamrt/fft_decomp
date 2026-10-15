#include "fft/battle_unit.h"

/* The runtime (display) record whose battle stats occupy unit slot battle_id, or 0. */
battle_unit_t* battle_unit_get_by_battle_id(u16 battle_id) {
    battle_unit_t* unit;

    unit = g_battle_unit_list_head;
    while (unit != 0) {
        if ((unit->stats != 0) && (unit->stats->battle_id == battle_id)) {
            return unit;
        }
        unit = unit->previous;
    }
    return 0;
}
