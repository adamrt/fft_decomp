/* A menu callback returning whether an ability list entry is not disabled. */

#include "fft/ability_list.h"
#include "psx/types.h"

/* Menu callback: 1 unless the entry's disabled bit (0x4000) is set. */
s32 ability_list_is_learned(s32 index) {
    return (g_ability_list_ability_entries[index] & ABILITY_LIST_ENTRY_DISABLED) == 0;
}
