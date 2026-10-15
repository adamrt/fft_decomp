#include "fft/battle_gfx.h"
#include "psx/types.h"

/* Restarts the unit graphics loader with one descriptor, for a unit an event adds
 * during the battle: the unit is put on the field
 * (battle_unit_set_enemy_level_data_by_battle_id) and queued at its map position
 * and facing with its sheet and palette, the caller's flags and runtime id 0xff,
 * which takes the first free runtime slot. Always returns 1. */
s32 battle_gfx_load_unit_graphics_by_battle_id(s32 battle_id, s32 flags) {
    unit_stats_t* unit;

    g_main_gfx_unit_graphics_load_queue.count = 0;
    g_battle_gfx_unit_graphics_load_state.file_kind = BATTLE_GFX_UNIT_LOAD_KIND_FIRST;
    g_battle_gfx_unit_graphics_load_state.phase = BATTLE_GFX_UNIT_LOAD_PHASE_START_READ;
    g_battle_gfx_unit_graphics_load_state.index = 0;
    g_battle_gfx_unit_graphics_load_state.buffer = 0;
    unit = battle_unit_get_stats_from_battle_id(battle_id);
    battle_unit_set_enemy_level_data_by_battle_id(battle_id);
    battle_gfx_append_unit_graphics_load_descriptor(unit->x, unit->position.y, unit->position.higher_elevation,
        unit->position.facing, unit->spritesheet_id, unit->job_portrait_palette, 0xFF, unit, flags);
    return 1;
}
