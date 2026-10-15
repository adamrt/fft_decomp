#include "fft/battle_unit.h"

/* Moves a unit's record to the head of the runtime unit list, so list walks reach it
 * first. Returns 1 when moved, 0 when no record has the runtime id. */
s32 battle_unit_move_to_head(u32 runtime_id) {
    battle_unit_t* unit;
    battle_unit_t* iterator;
    battle_unit_t* head;

    unit = battle_unit_get_by_runtime_id(runtime_id & 0xffff);
    if (unit != 0) {
        /* The 32-bit list-head pointer serves as a sentinel record: its previous field
         * (0x000) is the head link. */
        for (iterator = (battle_unit_t*)&g_battle_unit_list_head; iterator != 0; iterator = iterator->previous) {
            if (iterator->previous == unit) {
                iterator->previous = unit->previous;
                head = (battle_unit_t*)&g_battle_unit_list_head;
                unit->previous = head->previous;
                head->previous = unit;
                return 1;
            }
        }
        return 0;
    }
    return 0;
}
