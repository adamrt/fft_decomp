#include "fft/battle_gfx.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* Reloads a unit's palette with its weather and time-of-day tint and no tile
 * effect level, for the ResetPalette event instruction. An absent unit is
 * skipped. */
void battle_gfx_apply_weather_time_tile_palette_mod_by_runtime_id(u32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id);

    if (unit != 0) {
        battle_gfx_tint_unit_palette_for_weather_and_tile(unit, 0, 1);
    }
}

void battle_unit_set_movement_effect_suppression(u32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id & 0xffff);

    if (unit != 0) {
        unit->movement.suppress_sfx_and_landing_effects = 1;
    }
}

void battle_unit_clear_movement_effect_suppression(u32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id & 0xffff);

    if (unit != 0) {
        unit->movement.suppress_sfx_and_landing_effects = 0;
    }
}

/* The battle unit slot of a runtime record, or -1 when the record has no battle
 * stats. A runtime id without a record raises pointer exception 12 and also returns
 * -1. */
s32 battle_unit_get_battle_id_by_runtime_id(u32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id & 0xffff);

    if (unit != 0) {
        unit_stats_t* stats = unit->stats;

        if (stats != 0) {
            return stats->battle_id;
        }
    } else {
        main_system_handle_pointer_exception(12);
    }
    return -1;
}

/* The runtime id of the record whose battle stats occupy unit slot battle_id, or -1. */
s32 battle_unit_get_runtime_id_by_battle_id(u32 battle_id) {
    battle_unit_t* unit = g_battle_unit_list_head;

    while (unit != 0) {
        unit_stats_t* stats = unit->stats;

        if (stats != 0 && stats->battle_id == battle_id) {
            return unit->runtime_id;
        }
        unit = unit->previous;
    }
    return -1;
}
