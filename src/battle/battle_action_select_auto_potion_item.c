#include "fft/battle.h"
#include "fft/data.h"
#include "fft/main_runtime.h"
#include "psx/types.h"

s32 battle_action_select_auto_potion_item(battle_stats_t* unit) {
    if (unit->initial_team_flags & BATTLE_TEAM_MASK) {
        return ITEM_ID_POTION;
    }
    if (g_main_item_quantities[ITEM_ID_POTION] != 0) {
        return ITEM_ID_POTION;
    }
    if (g_main_item_quantities[ITEM_ID_HI_POTION] != 0) {
        return ITEM_ID_HI_POTION;
    }
    if (g_main_item_quantities[ITEM_ID_X_POTION] != 0) {
        return ITEM_ID_X_POTION;
    }
    return -1;
}
