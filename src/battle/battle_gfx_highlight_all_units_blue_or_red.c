#include "fft/battle.h"

void battle_gfx_highlight_all_units_blue_or_red(s32 mode) {
    battle_unit_misc_data_t* caster;
    battle_unit_misc_data_t* unit;
    s32 i;

    caster = battle_unit_get_casting_misc_data();
    for (i = 0; i < 0x10; i++) {
        unit = battle_unit_get_misc_data_by_misc_id(i & 0xFFFF);
        if (unit == 0) {
            continue;
        }
        if (unit->battle_data == 0) {
            continue;
        }
        switch (mode) {
        case 0xC:
            if ((unit->battle_data->team_flags & BATTLE_TEAM_MASK)
                != (caster->battle_data->team_flags & BATTLE_TEAM_MASK)) {
                battle_map_modify_palette(9, 4, 3, i, 0, 0x10, 0, 0);
            }
            continue;
        case 0xE:
            if ((unit->battle_data->team_flags & BATTLE_TEAM_MASK)
                == (caster->battle_data->team_flags & BATTLE_TEAM_MASK)) {
                battle_map_modify_palette(9, 4, 3, i, 0, 0, 0, 0x18);
            }
            continue;
        }
    }
}
