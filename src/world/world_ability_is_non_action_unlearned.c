#include "fft/menu.h"
#include "fft/world.h"
#include "psx/types.h"

/* Returns 1 for an ability of any type other than ABILITY_TYPE_DEFAULT (by
 * the type the "learned" callback cached) whose entry has bit 14 or 15 set;
 * 0 otherwise. */
s32 world_ability_is_non_action_unlearned(s32 index) {
    s32 result = 0;

    if (g_menu_ability_type != 0) {
        result = (g_menu_ability_entries[index] >> ABILITY_LIST_ENTRY_DISABLED_SHIFT) != 0;
    }
    return result;
}
