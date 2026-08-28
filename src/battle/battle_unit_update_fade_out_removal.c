#include "fft/battle.h"
#include "psx/types.h"

s32 battle_unit_update_fade_out_removal(battle_unit_misc_data_t* unit) {
    u8 modifier;

    modifier = unit->palette_modifier;
    if (modifier == 0) {
        return 0;
    }
    if (g_battle_gfx_palette_update_flags[unit->unit_id].pending != 0) {
        return 0;
    }
    switch (modifier) {
    case 1:
        unit->palette_modifier = 2;
        battle_gfx_start_misc_unit_palette_modulation(4, 4, unit->unit_id, -0x1f, -0x1f, -0x1f);
        return 0;
    case 2:
        if (unit->unit_id != g_battle_casting_misc_id) {
            unit->palette_modifier = 3;
            if (unit->battle_data != 0) {
                battle_unit_disable_remove(unit->battle_data->misc_unit_id);
            }
            battle_unit_remove_misc(unit);
            return 1;
        }
        battle_gfx_reset_unit_graphic_trigger(unit->unit_id);
        return 0;
    }
    return 0;
}
