#include "fft/bunit.h"
#include "fft/data.h"
#include "psx/types.h"

s32 bunit_ability_is_non_action_unlearned(s32 index) {
    s32 result;

    result = 0;
    if (g_bunit_ability_type != ABILITY_TYPE_DEFAULT) {
        result = (((u16*)g_bunit_ability_entries)[index] >> ABILITY_LIST_ENTRY_DISABLED_SHIFT) != 0;
    }
    return result;
}
