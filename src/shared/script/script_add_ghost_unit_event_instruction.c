#include "fft/battle_gfx.h"
#include "fft/battle_script.h"
#include "fft/battle_unit.h"
#include "fft/main.h"

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
