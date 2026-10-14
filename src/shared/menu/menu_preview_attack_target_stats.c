#include "fft/battle_action.h"
#include "fft/battle_menu.h"
#include "fft/menu.h"
#include "fft/thread.h"

/* Load the preview target, restore the active unit, then display its panel. */
void menu_preview_attack_target_stats(void) {
    s32 saved;

    if (g_thread_contexts[THREAD_SLOT_RIGHT_UNIT_STATS].task_id != THREAD_TASK_ATTACK_CASTER_STATS) {
        thread_wait_until_inactive(THREAD_SLOT_RIGHT_UNIT_STATS);
    }
    thread_yield();
    thread_yield();
    saved = g_menu_selected_unit.battle_id;
    /* The target loads the preview unit id unsigned (lhu). */
    g_menu_selected_unit.battle_id = g_menu_comparison_battle_id;
    menu_load_selected_unit();
    g_menu_selected_unit.battle_id = saved;
    g_menu_comparison_unit_fields.state.generic_monster = g_menu_selected_unit_fields.state.generic_monster;
    g_menu_comparison_unit_fields.state.ability_ids[2] = g_menu_selected_unit_fields.state.ability_ids[2];
    menu_load_selected_unit();
    menu_display_hovered_unit_stats(&g_menu_attack_target_stats_rect, UNIT_STATS_PANEL_ATTACK_TARGET,
        g_battle_menu_attack_target_stats_setup, &g_menu_comparison_unit_status);
}
