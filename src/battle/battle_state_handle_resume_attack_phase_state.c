#include "fft/battle.h"
#include "fft/battle_action.h"

/* ResumeAttackPhase game-state handler: show the remaining post-effect messages,
 * then finish the strike.
 *
 * Each closed message (command 7, 8 or 0xff) lets
 * action_resume_attack_phase_control open the next queued one. When none
 * remains and the casting unit's numeric display is idle, a continued attack
 * starts its next strike. Otherwise the SP2 buffer and VRAM slot are released,
 * the acting unit's data is stored and the units return to their status
 * animations. Outside the pre-action phase the action phase then advances (up to
 * the reacting-unit phase) and the next due action is announced; when none is
 * due, or in the pre-action phase, the JP and EXP rewards begin.
 *
 * The facing computed for the acting unit's own action is passed to
 * action_post_action_facing_stub, which is empty. */
void battle_state_handle_resume_attack_phase_state(void) {
    s32 facing;
    unit_stats_t* stats;
    s32 selected_command;
    battle_unit_t* unit;

    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_main_input_current_buttons);
    selected_command = menu_get_selected_command_address()->kind;
    if (selected_command >= 7 && (selected_command < 9 || selected_command == 0xff)) {
        g_action_post_action = 1;
    }
    unit = battle_unit_get_casting();
    if (g_action_post_action != 0 && action_resume_attack_phase_control() == 0 && unit->numeric_display.active == 0) {
        if (unit->strike.continue_attack != 0) {
            unit->continue_attack_count += 1;
            action_set_damage_display_type_based_on_ability();
            return;
        }
        if (g_battle_gfx_sp2_data != 0) {
            main_heap_free(g_battle_gfx_sp2_data);
            g_battle_gfx_sp2_data = 0;
        }
        battle_gfx_invalidate_sp2_vram_slot(unit);
        if (g_action_turn_event_type == BATTLE_TURN_EVENT_UNIT_READY && g_action_phase == ACTION_PHASE_ACTING_UNIT) {
            facing = unit->facing;
            if (facing < 0) {
                facing += 0x3ff;
            }
            facing = (u32)facing >> 10;
            stats = unit->stats;
            action_post_action_facing_stub(stats, facing & 0xff);
        }
        action_store_acting_unit_data(unit->stats);
        battle_unit_call_set_animation_based_on_status(unit);
        battle_unit_update_anim_display_for_all_targets(unit);
        battle_unit_get_source();
        if (g_action_phase != ACTION_PHASE_PRE_ACTION) {
            if (g_action_phase != ACTION_PHASE_REACTING_UNIT) {
                g_action_phase += 1;
            }
            if (battle_state_announce_next_charged_action() != 0) {
                return;
            }
        }
        action_handle_post_action_xp_jp_ability();
    }
}
