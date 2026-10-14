#include "fft/battle_action.h"
#include "fft/battle_menu.h"
#include "fft/battle_unit.h"
#include "fft/main.h"
#include "fft/menu.h"
#include "fft/thread.h"

/* Preview the pending action, then display its caster panel. */
void menu_preview_attack_caster_stats(void) {
    unit_stats_t* attacker;
    unit_stats_t* target;
    action_command_t* action;
    ability_data_t* ability_data;
    main_ability_secondary_pointer_t secondary;
    s16* selected;

    attacker = menu_get_selected_unit_stats();
    target = battle_unit_get_stats_from_battle_id(g_menu_comparison_battle_id);
    /* One address register serves both reads, as in the target. */
    selected = &g_menu_selected_unit.battle_id;
    action = battle_unit_get_runtime_action_command_by_battle_id(*selected);
    action->actor_id = *selected;
    if (action->ability_id < ABILITY_ID_ITEM_FIRST) {
        main_ability_calculate_pointers_and_type(action->ability_id & ABILITY_ID_MASK, &ability_data, &secondary);
    }
    /* Item IDs leave secondary unset before this range-flags read. */
    if (((secondary.range->flags_3 & ABILITY_SECONDARY_FLAG_3_PERSEVERE) && action->ability_id < ABILITY_ID_ITEM_FIRST)
        || action->skillset_id == SKILLSET_ID_EQUIP_CHANGE) {
        g_battle_menu_preview_target_action = 0;
    } else {
        g_thread_call_target = (void (*)(void))action_calculate_projected_effect;
        thread_call_on_main_stack(attacker, target, action);
        g_battle_menu_preview_target_action = &target->action;
    }
    thread_yield();
    thread_yield();
    menu_load_selected_unit();
    menu_display_hovered_unit_stats(&g_menu_attack_caster_stats_rect, UNIT_STATS_PANEL_ATTACK_CASTER,
        g_battle_menu_attack_caster_stats_setup, &g_menu_selected_unit_status);
}
