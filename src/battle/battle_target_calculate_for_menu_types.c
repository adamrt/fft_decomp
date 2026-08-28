#include "fft/battle.h"
#include "fft/data.h"
#include "fft/main_heap.h"
#include "fft/main_runtime.h"
#include "fft/unit_slots.h"
#include "psx/types.h"

/* Validates a copied action and runs the targeting routine for its menu type.
 *
 * Returns -1 for an absent actor, an out-of-range unit target or an invalid
 * ability, 2 for menu types with no targeting, otherwise 1 when the routine
 * returns 0, -1 when it returns -1, and 0 for any other result. The target
 * reads the actor's stats pointer in $a0 as battle_action_get_elemental_ability_id's
 * argument, so it is declared with one here. */
s32 battle_target_calculate_for_menu_types(const u8* source) {
    battle_ai_command_action_t action;
    battle_stats_t* unit;
    u16 ability_id;
    s32 mode;
    s32 result;

    main_util_copy_action_data(source, &action);
    mode = 1;
    if (action.unit_id >= BATTLE_UNIT_SLOT_COUNT) {
        return -1;
    }
    unit = &g_battle_unit_stats[action.unit_id];
    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return -1;
    }
    if (action.targeting_type == 6 && action.target_id >= BATTLE_UNIT_SLOT_COUNT) {
        return -1;
    }
    switch (g_main_action_menu_types_by_skillset[action.skillset]) {
    case ACTION_MENU_TYPE_DEFAULT:
    case ACTION_MENU_TYPE_MONSTER:
        ability_id = action.ability_id;
        break;
    case ACTION_MENU_TYPE_ITEM_INVENTORY:
    case ACTION_MENU_TYPE_WEAPON_INVENTORY:
    case ACTION_MENU_TYPE_ATTACK:
    case ACTION_MENU_TYPE_JUMP:
    case ACTION_MENU_TYPE_CHARGE:
        mode = 2;
        break;
    case ACTION_MENU_TYPE_ARITHMETICKS:
        result = battle_target_run_calculator(&action);
        mode = 0;
        break;
    case ACTION_MENU_TYPE_ELEMENTS:
        ability_id = battle_action_get_elemental_ability_id(unit);
        break;
    case ACTION_MENU_TYPE_KATANA_INVENTORY:
        if ((u32)(action.item_id - 0x26) >= 10) {
            return -1;
        }
        ability_id = action.item_id + 0x26;
        break;
    case ACTION_MENU_TYPE_BLANK_05:
    case ACTION_MENU_TYPE_UNKNOWN_0F:
        return -1;
    default:
        return 2;
    }
    if (mode == 1) {
        if (ability_id >= ABILITY_ID_ITEM_FIRST) {
            return -1;
        }
        action.ability_id = ability_id;
        result = battle_target_set_green_panels_for_action(&action);
    }
    if (mode == 2) {
        result = battle_target_disable_green_panel_on_all_but_target_tile((u8*)&action);
    }
    if (result == 0) {
        return 1;
    }
    return -(result == -1);
}
