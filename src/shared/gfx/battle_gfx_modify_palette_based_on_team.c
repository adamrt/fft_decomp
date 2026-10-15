#include "fft/battle_gfx.h"
#include "fft/battle_map.h"

/* Tint a unit's palette by its team: blue, red, green or light blue.
 *
 * `amount` passes through to map_modify_palette. */
void battle_gfx_modify_palette_based_on_team(battle_unit_t* unit, s32 amount) {
    if (unit == 0) {
        return;
    }
    if (unit->stats == 0) {
        return;
    }
    switch ((unit->stats->team_flags & BATTLE_TEAM_MASK) >> BATTLE_TEAM_SHIFT) {
    case BATTLE_TEAM_BLUE >> BATTLE_TEAM_SHIFT:
        map_modify_palette(MAP_COLOR_MODE_BASE_REPEAT, amount, MAP_CLUT_ROW_UNITS, unit->runtime_id, 0, 0, 0, 0x18);
        break;
    case BATTLE_TEAM_RED >> BATTLE_TEAM_SHIFT:
        map_modify_palette(MAP_COLOR_MODE_BASE_REPEAT, amount, MAP_CLUT_ROW_UNITS, unit->runtime_id, 0, 0x10, 0, 0);
        break;
    case BATTLE_TEAM_GREEN >> BATTLE_TEAM_SHIFT:
        map_modify_palette(MAP_COLOR_MODE_BASE_REPEAT, amount, MAP_CLUT_ROW_UNITS, unit->runtime_id, 0, 0, 0x10, 0);
        break;
    default:
        map_modify_palette(MAP_COLOR_MODE_BASE_REPEAT, amount, MAP_CLUT_ROW_UNITS, unit->runtime_id, 0, 0, 0x10, 0x10);
        break;
    }
}
