#include "fft/battle_ai.h"
#include "fft/unit_slots.h"

/* Return the first present unit's slot at these coordinates, or BATTLE_UNIT_ID_NONE if absent. */
s32 battle_ai_find_unit_at_coordinates(battle_ai_coords_t* coords) {
    s32 i;
    battle_stats_t* unit;

    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        unit = &g_battle_unit_stats[i];
        if (unit->entd_slot != BATTLE_ENTD_SLOT_NONE && unit->x == coords->bytes.x
            && unit->position.bits.y == coords->bytes.y && (unit->position.raw >> 15) == coords->bytes.elevation) {
            break;
        }
    }
    return i;
}
