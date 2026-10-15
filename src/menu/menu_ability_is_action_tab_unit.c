#include "fft/menu.h"

s32 menu_ability_is_action_tab(s32 unused_row) {
    return g_menu_ability_category == MENU_ABILITY_CATEGORY_ACTION;
}

/* Reaction abilities occupy category one. */
s32 menu_ability_is_reaction_tab(s32 unused_row) {
    return g_menu_ability_category == MENU_ABILITY_CATEGORY_REACTION;
}

/* Support abilities occupy category two. */
s32 menu_ability_is_support_tab(s32 unused_row) {
    return g_menu_ability_category == MENU_ABILITY_CATEGORY_SUPPORT;
}

/* Movement abilities occupy category three. */
s32 menu_ability_is_movement_tab(s32 unused_row) {
    return g_menu_ability_category == MENU_ABILITY_CATEGORY_MOVEMENT;
}
