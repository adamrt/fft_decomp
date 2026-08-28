#include "fft/battle.h"
#include "psx/types.h"
s32 battle_unit_count_from_misc_data(void) {
    s32 count = 0;
    u8* unit = (u8*)g_battle_unit_last_misc_data;
    if (unit != 0) {
        do {
            unit = *(u8**)unit;
            count++;
        } while (unit != 0);
    }
    return count;
}
