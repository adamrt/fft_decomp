#include "fft/battle.h"
#include "psx/types.h"

s32 battle_ai_add_unique_value_to_list(s32 mode, s16 value, s32 limit) {
    s32 idx;
    s32 i;

    if (mode == 0xFF) {
        g_battle_unique_value_list_length = 0;
        return 0;
    }
    if (mode == 0) {
        g_battle_unique_value_list_length = 1;
        g_battle_unique_value_list[0] = value;
        return 0;
    }
    idx = 0xFF;
    for (i = 0; i < g_battle_unique_value_list_length; i++) {
        if (g_battle_unique_value_list[i] == value) {
            idx = i;
            break;
        }
    }
    if (idx == 0xFF) {
        idx = g_battle_unique_value_list_length;
        g_battle_unique_value_list_length = idx + 1;
        g_battle_unique_value_list[idx] = value;
    }
    return idx >= limit;
}
