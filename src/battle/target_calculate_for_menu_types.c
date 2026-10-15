#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_effect.h"
#include "fft/battle_map.h"
#include "fft/battle_status.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

s32 target_calculate_for_menu_types(const action_command_t* source) {
    action_command_t action;
    unit_stats_t* unit;
    u16 ability_id;
    u8 item;
    s32 mode;
    s32 result;

    main_util_copy_action_data(source, &action);
    mode = 1;
    if (action.actor_id >= BATTLE_UNIT_SLOT_COUNT) {
        return -1;
    }
    unit = &g_battle_unit_stats[action.actor_id];
    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return -1;
    }
    if (action.targeting_type == BATTLE_TARGETING_TYPE_UNIT && action.target_id >= BATTLE_UNIT_SLOT_COUNT) {
        return -1;
    }
    switch (g_main_action_menu_types_by_skillset[action.skillset_id]) {
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
        result = target_run_calculator(&action);
        mode = 0;
        break;
    case ACTION_MENU_TYPE_ELEMENTS:
        ability_id = action_get_elemental_ability_id(unit);
        break;
    case ACTION_MENU_TYPE_KATANA_INVENTORY:
        item = action.item_or_equipment;
        if (item < ITEM_ID_ASURA_KNIFE || item > ITEM_ID_CHIRIJIRADEN) {
            return -1;
        }
        ability_id = (u8)action.item_or_equipment + (ABILITY_ID_DRAW_OUT_ASURA - ITEM_ID_ASURA_KNIFE);
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
        result = target_set_green_panels_for_action(&action);
    }
    if (mode == 2) {
        result = target_disable_green_panel_on_all_but_target_tile(&action);
    }
    if (result == 0) {
        return 1;
    }
    return -(result == -1);
}
