#include "fft/battle.h"

/* Tint a unit's palette by its team: blue, red, green or light blue.
 *
 * `amount` passes through to battle_map_modify_palette. */
void battle_gfx_modify_palette_based_on_team(battle_unit_misc_data_t* unit, s32 amount) {
    if (unit == 0) {
        return;
    }
    if (unit->battle_data == 0) {
        return;
    }
    switch ((unit->battle_data->team_flags & BATTLE_TEAM_MASK) >> 4) {
    case 0:
        battle_map_modify_palette(9, amount, 3, unit->unit_id, 0, 0, 0, 0x18);
        break;
    case 1:
        battle_map_modify_palette(9, amount, 3, unit->unit_id, 0, 0x10, 0, 0);
        break;
    case 2:
        battle_map_modify_palette(9, amount, 3, unit->unit_id, 0, 0, 0x10, 0);
        break;
    default:
        battle_map_modify_palette(9, amount, 3, unit->unit_id, 0, 0, 0x10, 0x10);
        break;
    }
}
