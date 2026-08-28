#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

void world_menu_preview_attack_target_stats(void) {
    s16* stored_unit;
    s32 saved;

    if (g_world_thread_contexts[12].task_id != 0x13) {
        world_thread_wait_until_inactive(0xC);
    }
    world_thread_yield();
    world_thread_yield();
    /* Local pointer keeps the stored-unit address in one base register. */
    stored_unit = &g_world_unit_view_battle_id;
    saved = *stored_unit;
    *stored_unit = g_world_unit_comparison_battle_id;
    world_unit_load_selected_into_editor();
    *stored_unit = saved;
    /* Unsigned halfword read of the record's leading flag: a (u16) cast on the
     * s16 field would emit lh + andi instead of the target's lhu. */
    g_world_unit_comparison_editor_panel_data.flag = *(u16*)&g_world_unit_editor_panel_data;
    g_world_unit_comparison_editor_panel_data.label_text_ids[2] = g_world_unit_editor_panel_data.label_text_ids[2];
    world_unit_load_selected_into_editor();
    world_menu_display_hovered_unit_stats(&g_world_menu_attack_target_stats_display, 2,
        g_world_menu_attack_target_stats_setup, &g_world_unit_comparison_status_billboard);
}
