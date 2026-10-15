/* Event unit graphics load and activation. */

#include "fft/battle_gfx.h"
#include "fft/battle_script.h"
#include "fft/battle_unit.h"
#include "fft/main.h"
#include "psx/types.h"

/* AddGhostUnit: queue the graphics load that creates an event-only unit on the
 * given unit slot.
 *
 * Restarts the unit-graphics loader at its first file kind and queues a single
 * descriptor with no battle unit behind it. map_height carries the instruction's
 * facing operand and portrait_id its spritesheet id; flags is the hidden flag.
 * Returns 0 without queuing when runtime_id is already in use, otherwise 1. */
s32 script_add_ghost_unit_event_instruction(
    s32 map_x, s32 map_y, s32 map_level, s16 facing, s16 spritesheet_id, s32 runtime_id, s32 flags) {
    s32 ret;

    if (battle_unit_get_by_runtime_id(runtime_id) != 0) {
        ret = 0;
    } else {
        g_main_gfx_unit_graphics_load_queue.count = 0;
        g_battle_gfx_unit_graphics_load_state.file_kind = BATTLE_GFX_UNIT_LOAD_KIND_FIRST;
        g_battle_gfx_unit_graphics_load_state.phase = BATTLE_GFX_UNIT_LOAD_PHASE_START_READ;
        g_battle_gfx_unit_graphics_load_state.index = 0;
        g_battle_gfx_unit_graphics_load_state.buffer = 0;
        battle_gfx_append_unit_graphics_load_descriptor(
            map_x, map_y, map_level, facing, spritesheet_id, 0, runtime_id, 0, flags);
        ret = 1;
    }
    return ret;
}

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

/* The underlying loader handles ordinary SPR as well as special SHP/SEQ.
 * Keep polling only its immediate-progress result, not deferred work. */
s32 battle_gfx_poll_unit_graphics_load(void) {
    s32 status;

    do {
        status = battle_gfx_step_queued_unit_graphics_load();
    } while (status == 2);
    return status;
}

/* Makes a unit visible: enables its drawing, triggers its sprite graphic and,
 * when it has battle stats, runs battle_unit_set_enemy_level_data_by_battle_id.
 * Returns 0 when no record has the runtime id. */
s32 battle_unit_draw_and_activate_by_runtime_id(u32 runtime_id) {
    battle_unit_t* unit;
    unit_stats_t* stats;

    unit = battle_unit_get_by_runtime_id(runtime_id & 0xffff);
    if (unit != 0) {
        stats = unit->stats;
        unit->draw_enabled = 1;
        unit->animation.trigger = 1;
        if (stats != 0) {
            battle_unit_set_enemy_level_data_by_battle_id(stats->battle_id);
        }
        return 1;
    }
    return 0;
}
