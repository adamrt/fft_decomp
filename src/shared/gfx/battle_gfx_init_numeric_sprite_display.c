#include "fft/battle_gfx.h"

/* Resets one of a unit's three numeric sprite displays to neutral color and no
 * parts. A display_section outside 0..2 leaves `display` unset; the only caller
 * passes 0, 1 and 2. */
battle_gfx_sprite_display_data_t* battle_gfx_init_numeric_sprite_display(s32 unit_index, s32 display_section) {
    battle_gfx_sprite_display_data_t* display;
    s32 i;

    switch (display_section) {
    case 0:
        display = &g_battle_units[unit_index].numeric_sprite_displays[0].header;
        break;
    case 1:
        display = &g_battle_units[unit_index].numeric_sprite_displays[1].header;
        break;
    case 2:
        display = &g_battle_units[unit_index].numeric_sprite_displays[2].header;
        break;
    }

    display->color.b = 0x80;
    display->color.g = 0x80;
    display->color.r = 0x80;
    for (i = 0; i < 1; i++)
        display->parts[i].flags = 0;
    display->part_count = 0;
    return display;
}
