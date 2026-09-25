#include "fft/battle.h"
#include "fft/battle_ai.h"
#include "fft/battle_move.h"
#include "psx/types.h"

extern void battle_unit_dismount_rider_and_update_display(battle_unit_misc_data_t* unit);
extern void battle_unit_init_coordinates(battle_unit_misc_data_t* unit);

/*
 * Advance a walking unit by one frame: the elaborate sibling of
 * battle_move_update_knockback_step at 0x8006db10, with the full 0x3c-entry
 * step-phase table instead of the knockback subset.
 *
 * Phase 0x11/0x15/0x19/0x1d ends a mount ride once the attack animation has
 * run out; phases 0x3b and 0x3c close out the path. When no step is active
 * the path byte count selects the ending: 0xfe dismounts, 0xff dismounts and
 * restarts the AI action, 0 or an exhausted path mounts the pending mount,
 * and otherwise the next path byte starts a step (flag 0x80 and 0x40 of
 * movement_flags pick their own starters, and bit 2 of the step byte picks the
 * climb speed).
 */
void battle_move_update_path_step(battle_unit_misc_data_t* unit) {
    u32 offset;
    u8 count;
    u8 flags;

    battle_move_get_current_and_destination_tiles(unit, &g_battle_move_current_tile, &g_battle_move_destination_tile);
    g_battle_move_step_value = unit->movement_value;
    switch (unit->centre_tile_offset) {
    case 0x11:
    case 0x15:
    case 0x19:
    case 0x1D:
        if (unit->animation_countdown == 0) {
            unit->centre_tile_offset++;
            battle_unit_store_animation_facing(0x1F, (s16)unit->facing, unit);
            battle_sound_play_movement_sfx(unit, 0x27);
            battle_unit_dismount_rider(unit);
        }
        break;
    case 0x14:
    case 0x18:
    case 0x1C:
    case 0x20:
        battle_move_update_knockback_after_animation(unit);
        break;
    case 0x12:
    case 0x16:
    case 0x1A:
    case 0x1E:
        battle_move_update_airborne_ascent_phase(unit);
        /* fallthrough */
    case 0x13:
    case 0x17:
    case 0x1B:
    case 0x1F:
        battle_move_finish_unit_step_at_tile_edge(unit);
        break;
    case 0x2D:
    case 0x31:
    case 0x35:
    case 0x39:
        battle_move_update_unit_step_to_destination_tile_entry_edge(unit);
        break;
    case 2:
    case 4:
    case 6:
    case 8:
    case 0xA:
    case 0xC:
    case 0xE:
    case 0x10:
        battle_move_update_unit_step_to_destination_tile_center(unit);
        break;
    case 0x2C:
    case 0x30:
    case 0x34:
    case 0x38:
        battle_move_update_float_step_to_destination_tile_center(unit);
        break;
    case 0x23:
    case 0x25:
    case 0x27:
    case 0x29:
        battle_move_update_unit_vertical_step_to_destination_tile_center(unit);
        break;
    case 1:
    case 3:
    case 5:
    case 7:
    case 9:
    case 0xB:
    case 0xD:
    case 0xF:
        battle_move_update_walking_step_at_tile_edge(unit);
        break;
    case 0x2B:
    case 0x2F:
    case 0x33:
    case 0x37:
        battle_move_update_float_step_to_current_tile_exit_edge(unit);
        break;
    case 0x22:
    case 0x24:
    case 0x26:
    case 0x28:
        battle_move_update_unit_step_to_current_tile_exit_edge(unit);
        break;
    case 0x3B:
        battle_unit_init_coordinates(unit);
        break;
    case 0x3C:
        battle_move_finalize_path_after_animation(unit);
        break;
    }
    if (unit->centre_tile_offset == 0) {
        count = unit->movement_path_count;
        if (count == 0xFE) {
            battle_unit_dismount_rider_and_update_display(unit);
            battle_effect_call_build_secondary_init(unit);
        } else if (count == 0xFF) {
            battle_unit_dismount_rider_and_update_display(unit);
            battle_effect_call_build_secondary_init(unit);
            unit->mount_byte = 0;
            battle_ai_init_selected_action();
        } else if (count == 0) {
            battle_unit_mount_onto_pending_mount(unit);
        } else if (unit->movement_path_offset < count) {
            if (unit->movement_path_offset == count - 1) {
                battle_move_set_unit_path_flag(unit);
            }
            offset = unit->movement_path_offset;
            unit->movement_path_offset = offset + 1;
            unit->movement_value = g_battle_move_step_value = unit->movement_path[offset];
            battle_move_set_unit_step_slope_scales(unit);
            flags = unit->movement_flags;
            if (flags & 0x80) {
                battle_move_start_fly_step(unit, (s32)g_battle_move_current_tile, (s32)g_battle_move_destination_tile);
            } else if (flags & 0x40) {
                battle_move_start_float_step(unit, g_battle_move_current_tile, g_battle_move_destination_tile);
            } else {
                switch ((g_battle_move_step_value >> 2) & 1) {
                case 0:
                    unit->step_speed = 0x2000;
                    /* The target passes the destination tile where the callee declares facing. */
                    ((void (*)(
                        battle_unit_misc_data_t*, const map_tile_t*, const map_tile_t*))battle_move_start_unit_step)(
                        unit, g_battle_move_current_tile, g_battle_move_destination_tile);
                    break;
                case 1:
                    /* The target also passes the destination tile to this two-parameter callee. */
                    ((void (*)(battle_unit_misc_data_t*, const map_tile_t*,
                        const map_tile_t*))battle_move_start_unit_step_at_climb_speed)(
                        unit, g_battle_move_current_tile, g_battle_move_destination_tile);
                    break;
                }
            }
        } else {
            battle_unit_mount_onto_pending_mount(unit);
        }
    }
}
