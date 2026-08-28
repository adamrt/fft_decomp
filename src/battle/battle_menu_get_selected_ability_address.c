#include "fft/battle.h"
#include "psx/types.h"

s32* battle_menu_get_selected_ability_address(void) {
    return &g_selected_ability;
}
