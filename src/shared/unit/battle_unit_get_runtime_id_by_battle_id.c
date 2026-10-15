#include "fft/battle_unit.h"

/* The runtime id of the record whose battle stats occupy unit slot battle_id, or -1. */
s32 battle_unit_get_runtime_id_by_battle_id(u32 battle_id) {
    battle_unit_t* unit = g_battle_unit_list_head;

    while (unit != 0) {
        unit_stats_t* stats = unit->stats;

        if (stats != 0 && stats->battle_id == battle_id) {
            return unit->runtime_id;
        }
        unit = unit->previous;
    }
    return -1;
}
