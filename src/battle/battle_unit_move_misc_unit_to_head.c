#include "fft/battle.h"

s32 battle_unit_move_misc_unit_to_head(u32 misc_id) {
    battle_unit_misc_data_t* unit;
    battle_unit_misc_data_t* previous;
    battle_unit_misc_data_t* iterator;
    battle_unit_misc_data_t* head;

    unit = battle_unit_get_misc_data_by_misc_id(misc_id & 0xffff);
    if (unit != 0) {
        /* The list-head word serves as a sentinel record: its previous field
         * (0x000) is the head link. */
        iterator = (battle_unit_misc_data_t*)&g_battle_unit_misc_list_head;
        while (iterator != 0) {
            previous = iterator->previous;
            if (previous == unit) {
                /* The target places the splice after the not-found return. */
                goto found;
            }
            iterator = previous;
        }
        return 0;
    found:
        iterator->previous = unit->previous;
        head = (battle_unit_misc_data_t*)&g_battle_unit_misc_list_head;
        unit->previous = head->previous;
        head->previous = unit;
        return 1;
    }
    return 0;
}
