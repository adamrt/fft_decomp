#include "fft/battle.h"

battle_gfx_sprite_display_data_t* battle_gfx_init_numeric_sprite_display(
    s32 unit_index, s32 display_section, battle_gfx_sprite_display_data_t* default_display) {
    battle_gfx_sprite_display_data_t* display;
    s32 i;

    display = default_display;
    switch (display_section) {
    case 0:
        display = (battle_gfx_sprite_display_data_t*)&g_battle_gfx_numeric_sprite_display_0[unit_index * 0x440];
        break;
    case 1:
        display = (battle_gfx_sprite_display_data_t*)&g_battle_gfx_numeric_sprite_display_1[unit_index * 0x440];
        break;
    case 2:
        display = (battle_gfx_sprite_display_data_t*)&g_battle_gfx_numeric_sprite_display_2[unit_index * 0x440];
        break;
    }

    display->blue = 0x80;
    display->green = 0x80;
    display->red = 0x80;
    for (i = 0; i < 1; i++)
        display->parts[i].flags = 0;
    display->part_count = 0;
    return display;
}
