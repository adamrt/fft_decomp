#include "fft/menu.h"
#include "fft/world.h"
#include "psx/types.h"

/* Report whether ability entry `index` counts as already learned: the learned
 * flag in menu 12, otherwise the inverse of the disabled flag. */
s32 world_menu_is_learn_ability_learned(s32 index) {
    if (g_world_formation_current_menu == WORLD_FORMATION_MENU_ABILITY_LEARN) {
        return g_menu_ability_entries[index] & ABILITY_LIST_ENTRY_LEARNED;
    }
    return (g_menu_ability_entries[index] & ABILITY_LIST_ENTRY_DISABLED) == 0;
}
