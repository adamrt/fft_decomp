#include "fft/battle.h"
#include "psx/types.h"

void battle_gfx_update_status_bubble_graphic_trigger(battle_unit_misc_data_t* unit) {
    if ((unit->status_flags_1_4 & BATTLE_MISC_STATUS_BUBBLE_MASK) != 0 || unit->unit_id == g_battle_casting_misc_id) {
        unit->status_bubble_active = 1;
        unit->status_bubble_timer = 0;
    } else {
        unit->status_bubble_active = 0;
    }
}
