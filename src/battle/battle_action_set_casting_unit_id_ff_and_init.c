#include "fft/battle.h"
#include "psx/types.h"

void battle_action_set_casting_unit_id_ff_and_init(void) {
    battle_action_set_casting_unit_id_ff();
    battle_menu_init_system_function(8, 0, 0xff, 0, 1);
}
