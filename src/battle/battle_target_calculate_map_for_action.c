#include "fft/battle.h"
#include "fft/data.h"
#include "fft/main_runtime.h"
#include "psx/types.h"

/*
 * Compute target tiles for the action and report its ability flags_3 byte.
 *
 * Attack, Charge, item-inventory, weapon-inventory, direct-targeting, and
 * weapon-range actions use the weapon/direct resolver. Random-fire selection
 * runs only after successful ordinary targeting outside AI simulation.
 */
s32 battle_target_calculate_map_for_action(battle_ai_command_action_t* action, u8* flags_3) {
    ability_secondary_data_t* ability;
    s32 menu_type;
    u8 flags_4;
    u8 flags_1;
    u8 flags_2;
    s32 result;
    s16 ability_id;

    flags_4 = 0;
    flags_1 = 0;
    menu_type = g_main_action_menu_types_by_skillset[action->skillset];
    flags_2 = 0;
    *flags_3 = 0;
    if ((menu_type & 0xff) == ACTION_MENU_TYPE_DEFAULT || (menu_type & 0xff) == ACTION_MENU_TYPE_MONSTER) {
        ability_id = action->ability_id;
        if (ability_id < ABILITY_ID_ITEM_FIRST) {
            ability = &g_main_ability_range_data[ability_id];
            *flags_3 = ability->flags_3;
            flags_4 = ability->flags_4;
            flags_1 = ability->flags_1;
            flags_2 = ability->flags_2;
        }
    }
    if ((menu_type & 0xff) == ACTION_MENU_TYPE_ATTACK || (menu_type & 0xff) == ACTION_MENU_TYPE_CHARGE
        || (u32)(menu_type - ACTION_MENU_TYPE_ITEM_INVENTORY) < 2
        || (flags_4 & ABILITY_SECONDARY_FLAG_4_DIRECT_TARGETING) || (flags_1 & ABILITY_SECONDARY_FLAG_1_WEAPON_RANGE)) {
        /* The target loads the same four arguments for both targeting calls;
         * the weapon validator reads only the action. */
        result = ((s32 (*)(battle_ai_command_action_t*, u8*, u8, u8))battle_target_validate_weapon_target)(
            action, flags_3, menu_type, flags_1);
    } else {
        result = ((s32 (*)(battle_ai_command_action_t*, u8*, u8, u8))battle_target_calculate_for_menu_types)(
            action, flags_3, menu_type, flags_1);
        if (result != -1 && (flags_2 & ABILITY_SECONDARY_FLAG_2_RANDOM_FIRE)
            && g_battle_action_state == BATTLE_ACTION_STATE_EXECUTE) {
            battle_target_select_random_tile_for_random_fire_abilities();
        }
    }
    return result;
}
