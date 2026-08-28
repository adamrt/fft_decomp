#include "fft/battle.h"
#include "fft/main_runtime.h"
#include "psx/types.h"

/* Returns the Elemental (Geomancy) ability for the surface under the unit when
 * the unit knows and can use it, otherwise 0. */
s32 battle_menu_load_elemental_abilities(s32 unit_id, u8 skillset) {
    battle_stats_t* unit;
    s16 abilities[0x14];
    u8 mp_costs[0x18];
    u8 ability_ct[0x18];
    u8 ability_flags[0x18];
    u8 turns[0x18];
    s32 count;
    s32 i;
    s16 terrain_ability;

    unit = battle_unit_get_existing_pointer(unit_id);
    if (unit == 0) {
        return 0;
    }
    if (skillset != SKILLSET_ID_ELEMENTAL) {
        return 0;
    }
    count = battle_menu_get_unit_skillset_ability_data(
        unit_id, SKILLSET_ID_ELEMENTAL, abilities, mp_costs, ability_ct, 1, ability_flags, turns);
    if (count == 0) {
        return 0;
    }
    terrain_ability
        = g_geomancy_terrain_ability_table[((u8*)g_battle_map_tile_data)[battle_map_calculate_location(unit) * 8]
            & MAP_SURFACE_MASK];
    for (i = 0; i < count; i++) {
        if (abilities[i] == terrain_ability) {
            return (u16)terrain_ability;
        }
    }
    return 0;
}
