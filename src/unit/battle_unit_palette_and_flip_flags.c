/* By-runtime-id palette-update suppression, the ResetPalette tint reload and the flip clear. */

#include "fft/battle_gfx.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

void battle_unit_clear_horizontal_flip_flag(u32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id & 0xffff);

    if (unit != 0) {
        unit->horizontal_flip_flag = 0;
    }
}

void battle_unit_set_palette_update_suppression(u32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id & 0xffff);

    if (unit != 0) {
        unit->movement.suppress_palette_update = 1;
    }
}

void battle_unit_clear_palette_update_suppression(u32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id & 0xffff);

    if (unit != 0) {
        unit->movement.suppress_palette_update = 0;
    }
}

/* Reloads a unit's palette with its weather and time-of-day tint and no tile
 * effect level, for the ResetPalette event instruction. An absent unit is
 * skipped. */
void battle_gfx_apply_weather_time_tile_palette_mod_by_runtime_id(u32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id);

    if (unit != 0) {
        battle_gfx_tint_unit_palette_for_weather_and_tile(unit, 0, 1);
    }
}
