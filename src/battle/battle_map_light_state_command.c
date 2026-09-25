#include "fft/battle.h"
#include "psx/types.h"

/* Map colour state commands, selected on command - 0x56: 0x56 applies the
 * background gradient pair, 0x5a the ambient colour and 0x63 the darkness
 * colour; the neighbouring commands save, restore or return those records.
 *
 * Only the GET arms assign `result`; the others return it uninitialized, as
 * the target does. */
u8* battle_map_light_state_command(s32 command, u8* data) {
    u8* result;

    switch (command) {
    case MAP_LIGHT_COMMAND_SET_BACKGROUND_GRADIENT:
        g_battle_map_background_gradient_colors.first = ((map_color_t*)data)[0];
        g_battle_map_background_gradient_colors.second = ((map_color_t*)data)[1];
        break;
    case MAP_LIGHT_COMMAND_GET_BACKGROUND_GRADIENT:
        result = (u8*)&g_battle_map_background_gradient_colors;
        break;
    case MAP_LIGHT_COMMAND_SAVE_BACKGROUND_GRADIENT:
        g_battle_map_saved_background_gradient_first = g_battle_map_background_gradient_colors.first;
        g_battle_map_saved_background_gradient_second = g_battle_map_background_gradient_colors.second;
        break;
    case MAP_LIGHT_COMMAND_RESTORE_BACKGROUND_GRADIENT:
        g_battle_map_background_gradient_colors.first = g_battle_map_saved_background_gradient_first;
        g_battle_map_background_gradient_colors.second = g_battle_map_saved_background_gradient_second;
        break;
    case MAP_LIGHT_COMMAND_SET_AMBIENT_COLOR:
        g_battle_map_back_color_red = data[0];
        g_battle_map_back_color_green = data[1];
        g_battle_map_back_color_blue = data[2];
        SetBackColor(g_battle_map_back_color_red, g_battle_map_back_color_green, g_battle_map_back_color_blue);
        break;
    case MAP_LIGHT_COMMAND_SAVE_AMBIENT_COLOR:
        g_battle_map_saved_back_color_red = g_battle_map_back_color_red;
        g_battle_map_saved_back_color_green = g_battle_map_back_color_green;
        g_battle_map_saved_back_color_blue = g_battle_map_back_color_blue;
        break;
    case MAP_LIGHT_COMMAND_RESTORE_AMBIENT_COLOR:
        g_battle_map_back_color_red = g_battle_map_saved_back_color_red;
        g_battle_map_back_color_green = g_battle_map_saved_back_color_green;
        g_battle_map_back_color_blue = g_battle_map_saved_back_color_blue;
        break;
    case MAP_LIGHT_COMMAND_GET_AMBIENT_COLOR: {
        u8* packed = g_battle_map_back_color_bytes;

        packed[0] = g_battle_map_back_color_red;
        g_battle_map_back_color_bytes[1] = g_battle_map_back_color_green;
        g_battle_map_back_color_bytes[2] = g_battle_map_back_color_blue;
        result = packed;
        break;
    }
    case MAP_LIGHT_COMMAND_SET_DARKNESS_COLOR:
        g_battle_map_light_direction[0] = data[0];
        g_battle_map_light_direction[1] = data[1];
        g_battle_map_light_direction[2] = data[2];
        break;
    case MAP_LIGHT_COMMAND_RESET_DARKNESS_COLOR:
        g_battle_map_light_direction[0] = 0;
        g_battle_map_light_direction[1] = 0;
        g_battle_map_light_direction[2] = 1;
        break;
    case MAP_LIGHT_COMMAND_GET_DARKNESS_COLOR:
        result = g_battle_map_light_direction;
        break;
    }
    return result;
}
