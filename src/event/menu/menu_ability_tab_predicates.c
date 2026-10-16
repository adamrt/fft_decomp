/* Ability category tab tests. */

#include "fft/job.h"
#include "psx/types.h"

/* Row callback: return whether the ability list shows the action category; the row is ignored. */
s32 menu_ability_is_action_tab(s32 unused_row) {
    return g_menu_ability_category == MENU_ABILITY_CATEGORY_ACTION;
}

/* Row callback: return whether the ability list shows the reaction category; the row is ignored. */
s32 menu_ability_is_reaction_tab(s32 unused_row) {
    return g_menu_ability_category == MENU_ABILITY_CATEGORY_REACTION;
}

/* Row callback: return whether the ability list shows the support category; the row is ignored. */
s32 menu_ability_is_support_tab(s32 unused_row) {
    return g_menu_ability_category == MENU_ABILITY_CATEGORY_SUPPORT;
}

/* Row callback: return whether the ability list shows the movement category; the row is ignored. */
s32 menu_ability_is_movement_tab(s32 unused_row) {
    return g_menu_ability_category == MENU_ABILITY_CATEGORY_MOVEMENT;
}
