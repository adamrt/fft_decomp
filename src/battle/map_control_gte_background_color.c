#include "fft/battle_map.h"

/* GTE back color commands for the map's ambient color.
 *
 * SET stores the three components and loads them into the GTE, and returns the
 * address of the stored 32-bit red component; while the back color is disabled it does
 * neither and returns 0. GET returns that address. DISABLE loads black and blocks
 * SET; RESTORE reloads the stored color and lifts the block.
 *
 * DISABLE, RESTORE and a command with no case return the unset `result`. Every
 * caller issues SET and ignores the result. */
s32* map_control_gte_background_color(s32 command, s32 value_1, s32 value_2, s32 value_3) {
    /* Left unset on the DISABLE, RESTORE and default paths. */
    s32* result;

    switch (command) {
    case BATTLE_BACKGROUND_COLOR_SET: {
        s32* stored_values;

        if (g_map_back_color_disabled == 1) {
            return 0;
        }
        stored_values = &g_map_back_color_red;
        *stored_values = value_1;
        g_map_back_color_green = value_2;
        g_map_back_color_blue = value_3;
        SetBackColor(value_1, value_2, value_3);
        return stored_values;
    }
    case BATTLE_BACKGROUND_COLOR_GET:
        result = &g_map_back_color_red;
        break;
    case BATTLE_BACKGROUND_COLOR_DISABLE:
        SetBackColor(0, 0, 0);
        g_map_back_color_disabled = 1;
        return result;
    case BATTLE_BACKGROUND_COLOR_RESTORE:
        SetBackColor(g_map_back_color_red, g_map_back_color_green, g_map_back_color_blue);
        g_map_back_color_disabled = 0;
        return result;
    }
    return result;
}
