#include "fft/battle.h"
#include "psx/types.h"

/* Resolves the queued in-between-turn event (mimic, ability cast, traps).
 *
 * The queue word's high byte selects the action type (g_action_type) and its
 * low byte the unit; BATTLE_TURN_EVENT_NONE means nothing is queued. Unit- and
 * ability-ready events resolve an ability, while action-result events dispatch
 * the queued per-unit result. The meaning of 0x400 remains uncertain. */
void battle_action_run_between_turn_events(void) {
    battle_unit_misc_data_t* unit;
    s32 action;
    s32 misc_id;

    action = battle_action_run_between_turn_control(0);
    misc_id = action & 0xff;
    g_action_type = action & 0xff00;
    if (g_action_type == BATTLE_TURN_EVENT_NONE) {
        return;
    }
    unit = battle_unit_get_misc_data_by_battle_id(misc_id);
    if (unit != 0) {
        g_battle_casting_misc_id = unit->unit_id;
    } else {
        main_system_handle_pointer_exception(12);
    }
    switch (g_action_type) {
    case BATTLE_TURN_EVENT_UNIT_READY:
    case BATTLE_TURN_EVENT_ABILITY_READY:
        battle_unit_animate_and_set_enemy_level_data_by_misc_id(unit->unit_id);
        break;
    case BATTLE_TURN_EVENT_ACTION_RESULT:
        unit->pending_attack_result = battle_action_finalize_attack_and_flag_reactions(misc_id);
        if (unit->pending_attack_result == -1) {
            battle_unit_find_relocation_tile(misc_id, &unit->dismount);
            battle_unit_set_map_coords_after_death_dismount(unit);
        }
        break;
    case BATTLE_TURN_EVENT_UNKNOWN_0400:
        unit->pending_attack_result = battle_action_finalize_attack_and_flag_reactions(misc_id);
        if (unit->pending_attack_result == -1) {
            battle_unit_find_relocation_tile(misc_id, &unit->dismount);
            battle_unit_set_map_coords_after_death_dismount(unit);
        }
        battle_gfx_prepare_post_action_display_by_misc_id(unit->unit_id);
        battle_unit_update_display_by_misc_id(unit->unit_id);
        break;
    case BATTLE_TURN_EVENT_MIME:
        break;
    case 0x600: /* a label above 0x500 on the error path makes GCC split the upper
                   tree at 0x500 as the target does; nothing compares it, so the
                   original value is not recoverable */
    default:
        main_system_handle_pointer_exception(20);
        break;
    }
    unit = battle_unit_get_source_misc_data();
    if (unit != 0) {
        unit->ability_ct_resolved = 0;
        if (g_action_type != BATTLE_TURN_EVENT_UNKNOWN_0400) {
            battle_target_move_cursor_to_unit(unit);
        }
    }
}
