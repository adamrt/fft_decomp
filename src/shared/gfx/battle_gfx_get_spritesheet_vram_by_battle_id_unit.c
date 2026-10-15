#include "fft/battle_gfx.h"
#include "fft/battle_move.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* The VRAM spritesheet slot of the displayed unit with this battle id, or -1 when
 * no runtime record belongs to it. */
s32 battle_gfx_get_spritesheet_vram_by_battle_id(u32 battle_id) {
    battle_unit_t* unit = g_battle_unit_list_head;

    while (unit != 0) {
        unit_stats_t* stats = unit->stats;

        if (stats != 0 && stats->battle_id == battle_id) {
            return unit->spritesheet_vram_slot;
        }
        unit = unit->previous;
    }
    return -1;
}

/* The command action record (command_state.ai.data.action) of the unit in slot
 * battle_id, or 0 when it has no runtime record. */
action_command_t* battle_unit_get_runtime_action_command_by_battle_id(u32 battle_id) {
    battle_unit_t* unit = battle_unit_get_by_battle_id(battle_id & 0xffff);

    if (unit == 0) {
        return 0;
    }
    return &unit->command_state.ai.data.action;
}

/* 1 while the runtime unit still has a movement path. A missing unit raises pointer
 * exception 12 and reports 0. */
s32 move_is_unit_moving_by_runtime_id(u32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id & 0xffff);

    if (unit == 0) {
        main_system_handle_pointer_exception(12);
        return 0;
    }
    return unit->path.count != 0;
}

/* TempWeapon event instruction: battle_gfx_set_thrown_item_graphic_palette for a
 * runtime id. An absent unit is skipped. */
void battle_gfx_set_thrown_item_palette_by_runtime_id(u32 item_id, u32 runtime_id) {
    battle_unit_t* unit;

    unit = battle_unit_get_by_runtime_id(runtime_id);
    if (unit != 0) {
        battle_gfx_set_thrown_item_graphic_palette(item_id, unit);
    }
}
