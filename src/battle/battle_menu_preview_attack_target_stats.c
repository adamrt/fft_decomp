#include "fft/battle.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

extern world_unit_editor_panel_data_t g_battle_unit_editor_panel_data;
extern world_unit_editor_panel_data_t g_battle_unit_comparison_editor_panel_data;

void battle_menu_preview_attack_target_stats(void) {
    s16* sel;
    s32 saved;

    if (g_battle_thread_contexts[12].task_id != 0x13) {
        battle_thread_wait_until_inactive(0xC);
    }
    battle_thread_yield();
    battle_thread_yield();
    sel = &g_battle_active_turn_unit.battle_id;
    saved = *sel;
    /* The target loads the preview unit id unsigned (lhu). */
    *sel = *(u16*)&g_battle_preview_target_unit_id;
    battle_menu_store_units_small_in_battle_display_data();
    *sel = saved;
    g_battle_unit_comparison_editor_panel_data.flag = g_battle_unit_editor_panel_data.flag;
    g_battle_unit_comparison_editor_panel_data.label_text_ids[2] = g_battle_unit_editor_panel_data.label_text_ids[2];
    battle_menu_store_units_small_in_battle_display_data();
    battle_menu_display_hovered_unit_stats(&g_battle_menu_attack_target_stats_display, 2,
        (menu_number_entry_t*)g_battle_menu_attack_target_stats_setup, &g_battle_menu_status_billboard);
}
