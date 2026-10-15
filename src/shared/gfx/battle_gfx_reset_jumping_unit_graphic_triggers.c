#include "fft/battle_gfx.h"

/* Hides every unit that has the Jump status. */
void battle_gfx_reset_jumping_unit_graphic_triggers(void) {
    battle_unit_t* unit = g_battle_unit_list_head;

    while (unit != 0) {
        if ((unit->status_flags_5_6 & BATTLE_UNIT_STATUS_JUMP) != 0) {
            battle_gfx_reset_unit_graphic_trigger(unit->runtime_id);
        }
        unit = unit->previous;
    }
}
