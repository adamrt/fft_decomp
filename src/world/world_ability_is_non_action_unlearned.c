#include "fft/data.h"
#include "fft/world.h"
#include "psx/types.h"

s32 world_ability_is_non_action_unlearned(s32 index) {
    s32 result = 0;

    if (g_world_ability_selected_type != 0) {
        result = ((u16)g_world_ability_entries[index] >> ABILITY_LIST_ENTRY_DISABLED_SHIFT) != 0;
    }
    return result;
}
