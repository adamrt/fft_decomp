#include "psx/types.h"

void battle_unit_update_display_by_misc_id(u16 misc_id) {
    battle_unit_update_display(battle_unit_get_misc_data_by_misc_id(misc_id));
}
