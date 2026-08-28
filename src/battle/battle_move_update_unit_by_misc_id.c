#include "fft/battle.h"
#include "psx/types.h"

void battle_move_update_unit_by_misc_id(void) {
    s32 i;
    battle_unit_misc_data_t* unit;

    i = 0;
    do {
        unit = battle_unit_get_misc_data_by_misc_id((u16)i);
        if (unit != 0) {
            if (unit->movement_path_count != 0) {
                battle_move_update_path_step(unit);
            }
        }
        i++;
    } while (i < 0x10);
}
