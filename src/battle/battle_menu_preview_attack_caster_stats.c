#include "fft/battle.h"
#include "fft/data.h"
#include "fft/world.h"
#include "psx/types.h"

/* Signed view of battle_ai_command_action_t: the ability id is read with a
 * signed halfword load here. */
typedef world_unit_command_action_t battle_preview_action_t;

/*
 * Attack-preview caster panel: bind the selected unit's pending action to the
 * previewed target, run the preview calculation on the main stack unless the
 * ability's secondary flag 0x04 is set or the skillset changes equipment, and publish the
 * target's action results through g_battle_menu_preview_target_action (0 when skipped).
 */
void battle_menu_preview_attack_caster_stats(void) {
    battle_stats_t* attacker;
    battle_stats_t* target;
    battle_preview_action_t* action;
    u8* ability_data;
    ability_secondary_data_t* secondary;
    s16* selected;

    attacker = battle_unit_get_attacker_data_pointer();
    target = battle_unit_get_stats_from_battle_id(g_battle_preview_target_unit_id);
    /* One address register serves both reads, as in the target. */
    selected = &g_battle_active_turn_unit.battle_id;
    action = (battle_preview_action_t*)battle_unit_get_target_id_ptr_by_battle_id(*selected);
    action->unit_id = *selected;
    if (action->ability_id < ABILITY_ID_ITEM_FIRST) {
        main_ability_calculate_pointers_and_type(action->ability_id & 0x1ff, &ability_data, (u8**)&secondary);
    }
    if (((secondary->flags_3 & ABILITY_SECONDARY_FLAG_3_PERSEVERE) && action->ability_id < ABILITY_ID_ITEM_FIRST)
        || action->skillset == SKILLSET_ID_EQUIP_CHANGE) {
        g_battle_menu_preview_target_action = 0;
    } else {
        g_battle_thread_call_target = (void (*)(void))battle_action_calculate_projected_effect;
        battle_thread_call_on_main_stack(attacker, target, action);
        g_battle_menu_preview_target_action = &target->action;
    }
    battle_thread_yield();
    battle_thread_yield();
    battle_menu_store_units_small_in_battle_display_data();
    battle_menu_display_hovered_unit_stats(&g_battle_menu_attack_caster_stats_display, 1,
        (menu_number_entry_t*)g_battle_menu_attack_caster_stats_setup, &g_battle_menu_active_turn_banner);
}
