#include "fft/battle.h"
#include "psx/types.h"

void battle_unit_update_display_by_battle_id(u16 battle_id) {
    battle_unit_update_display(battle_unit_get_misc_data_by_battle_id(battle_id));
}
