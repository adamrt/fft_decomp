#include "fft/battle.h"
#include "fft/main_runtime.h"
#include "fft/world.h"
#include "psx/types.h"

void world_menu_preview_attack_caster_stats(void) {
    battle_stats_t* stored;
    battle_stats_t* target;
    world_unit_command_action_t* action;
    u8* ability_data;
    u8* secondary_data;
    s16 ability_id;
    s16* stored_unit;

    stored = world_unit_get_battle_stats_for_stored();
    target = battle_unit_get_stats_from_battle_id(g_world_unit_comparison_battle_id);
    /* Local pointer keeps the stored-unit address in one base register. */
    stored_unit = &g_world_unit_view_battle_id;
    action = (world_unit_command_action_t*)battle_unit_get_target_id_ptr_by_battle_id(*stored_unit);
    action->unit_id = *stored_unit;
    ability_id = action->ability_id;
    if (action->ability_id < ABILITY_ID_ITEM_FIRST) {
        main_ability_calculate_pointers_and_type(ability_id & 0x1FF, &ability_data, &secondary_data);
    }
    if (((secondary_data[5] & 4) && action->ability_id < ABILITY_ID_ITEM_FIRST)
        || action->skillset == SKILLSET_ID_EQUIP_CHANGE) {
        g_world_menu_preview_target_action = 0;
    } else {
        g_world_thread_call_target = (void (*)(void))battle_action_calculate_projected_effect;
        world_thread_call_on_main_stack(stored, target, action);
        g_world_menu_preview_target_action = (s32)target + 0x18C;
    }
    world_thread_yield();
    world_thread_yield();
    world_unit_load_selected_into_editor();
    world_menu_display_hovered_unit_stats(&g_world_menu_attack_caster_stats_display, 1,
        g_world_menu_attack_caster_stats_setup, &g_world_unit_selected_status_billboard);
}
