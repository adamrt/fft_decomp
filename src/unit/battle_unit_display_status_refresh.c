/* Pending status masks applied to the display state. */

#include "fft/battle.h"
#include "fft/battle_ai.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_status.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* Apply a unit's pending status add/remove masks and refresh its display state. */
void battle_unit_update_display(battle_unit_t* unit) {
    map_tile_t* tile;

    if (unit != 0) {
        tile = map_get_tile_data_pointer(unit->map_x, unit->map_y, unit->map_z);
        unit->status_flags_1_4 &= ~unit->statuses_to_remove_1_4;
        unit->status_flags_5_6 &= ~unit->statuses_to_remove_5_6;
        unit->status_flags_1_4 |= unit->statuses_to_add_1_4;
        unit->status_flags_5_6 |= unit->statuses_to_add_5_6;
        battle_unit_set_map_coords_after_death_dismount(unit);
        battle_gfx_update_sprite_transparency_flag(unit);
        battle_gfx_update_status_bubble_graphic_trigger(unit);
        if (g_battle_state_animation_continue_check != 0) {
            battle_gfx_check_tile_status_palette_mod(unit, tile->flags.shading, 0);
        } else {
            battle_gfx_check_tile_status_palette_mod(unit, tile->flags.shading, 1);
        }
        battle_unit_apply_poach_morbol_transformation(unit);
        battle_unit_set_animation_based_on_status(unit);
        battle_unit_update_animation_for_status_changes(unit);
        battle_gfx_apply_status_spritesheet_change(unit, tile->flags.shading);
        battle_unit_update_float_and_jump_height(unit);
        unit->statuses_to_remove_5_6 = 0;
        unit->statuses_to_remove_1_4 = 0;
        unit->statuses_to_add_5_6 = 0;
        unit->statuses_to_add_1_4 = 0;
        unit->item_display_active = 0;
    }
}

void battle_unit_update_display_by_runtime_id(s32 runtime_id) {
    battle_unit_update_display(battle_unit_get_by_runtime_id(runtime_id));
}

/* battle_unit_update_display for the unit in slot battle_id. No retail code calls
 * it. */
void battle_unit_update_display_by_battle_id(u16 battle_id) {
    battle_unit_update_display(battle_unit_get_by_battle_id(battle_id));
}

/*
 * Queue the renderer-side status flags associated with a simulation status.
 *
 * The status argument is the callback-table index (canonical status ID + 1).
 * Crystal removal deliberately stages the Chicken transformation bit, as in
 * the original transition logic. Rebuilding the unit's AI ability data when
 * Frog changes keeps its usable-ability list synchronized with the graphics
 * transition.
 */
s32 status_queue_runtime_graphics_flag_change(s32 status, s32 enabled, s32 battle_id) {
    battle_unit_t* unit;

    unit = battle_unit_get_by_battle_id(battle_id & 0xffff);
    if (unit == 0) {
        return 0;
    }
    switch (enabled) {
    case 0:
        switch (status) {
        case STATUS_HANDLER_INDEX(STATUS_ID_CRYSTAL):
        case STATUS_HANDLER_INDEX(STATUS_ID_CHICKEN):
            unit->statuses_to_remove_5_6 |= BATTLE_UNIT_STATUS_CHICKEN;
            unit->statuses_to_add_5_6 &= ~BATTLE_UNIT_STATUS_CHICKEN;
            break;
        case STATUS_HANDLER_INDEX(STATUS_ID_FROG):
            unit->statuses_to_remove_5_6 |= BATTLE_UNIT_STATUS_FROG;
            unit->statuses_to_add_5_6 &= ~BATTLE_UNIT_STATUS_FROG;
            ai_init_unit_abilities(battle_id);
            return 1;
        case STATUS_HANDLER_INDEX(STATUS_ID_JUMP):
        case STATUS_HANDLER_INDEX(STATUS_ID_FLOAT):
        case STATUS_HANDLER_INDEX(STATUS_ID_TRANSPARENT):
            unit->statuses_to_remove_5_6 |= g_battle_unit_status_mask_by_handler_index[status];
            unit->statuses_to_add_5_6 &= ~g_battle_unit_status_mask_by_handler_index[status];
            break;
        case STATUS_HANDLER_INDEX(STATUS_ID_UNNAMED_00):
        case STATUS_HANDLER_INDEX(STATUS_ID_INVITE):
        case STATUS_HANDLER_INDEX(STATUS_ID_TREASURE):
        case STATUS_HANDLER_INDEX(STATUS_ID_WALL):
            break;
        default:
            unit->statuses_to_remove_1_4 |= g_battle_unit_status_mask_by_handler_index[status];
            unit->statuses_to_add_1_4 &= ~g_battle_unit_status_mask_by_handler_index[status];
            break;
        }
        break;
    case 1:
        switch (status) {
        case STATUS_HANDLER_INDEX(STATUS_ID_CRYSTAL):
            unit->statuses_to_add_5_6 |= BATTLE_UNIT_STATUS_CRYSTAL;
            unit->statuses_to_remove_5_6 &= ~BATTLE_UNIT_STATUS_CRYSTAL;
            break;
        case STATUS_HANDLER_INDEX(STATUS_ID_CHICKEN):
            unit->statuses_to_add_5_6 |= BATTLE_UNIT_STATUS_CHICKEN;
            unit->statuses_to_remove_5_6 &= ~BATTLE_UNIT_STATUS_CHICKEN;
            break;
        case STATUS_HANDLER_INDEX(STATUS_ID_FROG):
            unit->statuses_to_add_5_6 |= BATTLE_UNIT_STATUS_FROG;
            unit->statuses_to_remove_5_6 &= ~BATTLE_UNIT_STATUS_FROG;
            ai_init_unit_abilities(battle_id);
            return 1;
        case STATUS_HANDLER_INDEX(STATUS_ID_TREASURE):
            unit->statuses_to_add_5_6 |= BATTLE_UNIT_STATUS_TREASURE;
            unit->statuses_to_remove_5_6 &= ~BATTLE_UNIT_STATUS_TREASURE;
            break;
        case STATUS_HANDLER_INDEX(STATUS_ID_JUMP):
        case STATUS_HANDLER_INDEX(STATUS_ID_FLOAT):
        case STATUS_HANDLER_INDEX(STATUS_ID_TRANSPARENT):
            unit->statuses_to_add_5_6 |= g_battle_unit_status_mask_by_handler_index[status];
            unit->statuses_to_remove_5_6 &= ~g_battle_unit_status_mask_by_handler_index[status];
            break;
        case STATUS_HANDLER_INDEX(STATUS_ID_UNNAMED_00):
        case STATUS_HANDLER_INDEX(STATUS_ID_INVITE):
        case STATUS_HANDLER_INDEX(STATUS_ID_WALL):
            break;
        default:
            unit->statuses_to_add_1_4 |= g_battle_unit_status_mask_by_handler_index[status];
            unit->statuses_to_remove_1_4 &= ~g_battle_unit_status_mask_by_handler_index[status];
            break;
        }
        break;
    case 2:
        /* No effect, but the label makes 1 the root of the target's
         * compare tree. */
        break;
    }
    return 1;
}
