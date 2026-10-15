#include "fft/battle_unit.h"

void battle_unit_clear_all(void) {
    battle_unit_t* unit;

    unit = g_battle_unit_list_head;
    while (unit != 0) {
        battle_unit_remove(unit);
        unit = unit->previous;
    }
}
