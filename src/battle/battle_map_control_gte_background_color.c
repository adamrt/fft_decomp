#include "fft/battle.h"

s32* battle_map_control_gte_background_color(s32 command, s32 value_1, s32 value_2, s32 value_3) {
    /* Matching limitation: commands 11/12 and the default path return this
     * uninitialized local. The target has no defined result on those paths;
     * this C expression remains undefined even when callers ignore it. */
    s32* result;

    switch (command) {
    case BATTLE_BACKGROUND_COLOR_SET: {
        s32* stored_values;

        if (g_battle_map_back_color_disabled == 1) {
            return 0;
        }
        stored_values = &g_battle_map_back_color_red;
        *stored_values = value_1;
        g_battle_map_back_color_green = value_2;
        g_battle_map_back_color_blue = value_3;
        SetBackColor(value_1, value_2, value_3);
        return stored_values;
    }
    case BATTLE_BACKGROUND_COLOR_GET:
        result = &g_battle_map_back_color_red;
        break;
    case BATTLE_BACKGROUND_COLOR_DISABLE:
        SetBackColor(0, 0, 0);
        g_battle_map_back_color_disabled = 1;
        return result;
    case BATTLE_BACKGROUND_COLOR_RESTORE:
        SetBackColor(g_battle_map_back_color_red, g_battle_map_back_color_green, g_battle_map_back_color_blue);
        g_battle_map_back_color_disabled = 0;
        return result;
    }
    return result;
}
