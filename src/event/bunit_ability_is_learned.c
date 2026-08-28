#include "fft/bunit.h"
#include "fft/data.h"
#include "psx/types.h"

s32 bunit_ability_is_learned(s32 index) {
    return ((((u16*)g_bunit_ability_entries)[index] >> ABILITY_LIST_ENTRY_DISABLED_SHIFT) ^ 1) & 1;
}
