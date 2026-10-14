#include "fft/battle_gfx.h"
#include "fft/battle_map.h"

/* Resets a unit's status bubble display to one 14 x 12 part at (0, 0xb0) of the
 * FRAME.BIN page with CLUT 7 of the sprite row, neutral color and scale ONE. */
battle_gfx_sprite_display_data_t* battle_gfx_init_status_bubble_sprite_display(s32 unit_index) {
    battle_gfx_sprite_display_data_t* display;
    s32 i;

    display = &g_battle_units[unit_index].status_bubble_sprite_display.header;
    display->color.b = 0x80;
    display->color.g = 0x80;
    display->color.r = 0x80;
    display->scale_y = ONE;
    display->scale_x = ONE;
    display->tpage = BATTLE_GFX_TPAGE_FRAME;
    display->clut = MAP_CLUT(MAP_CLUT_ROW_SPRITES, 7);
    display->rotation = 0;
    display->part_count = 1;
    for (i = 0; i < 1; i++) {
        display->parts[i].y_shift = 0;
        display->parts[i].x_shift = 0;
        display->parts[i].width = 0xe;
        display->parts[i].height = 0xc;
        display->parts[i].u = 0;
        display->parts[i].v = 0xb0;
        display->parts[i].flags = 0;
    }
    return display;
}
