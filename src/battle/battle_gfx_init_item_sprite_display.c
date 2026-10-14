#include "fft/battle_gfx.h"
#include "fft/battle_map.h"

/* Resets a unit's item sprite display to one 16 x 16 part of the ITEM.BIN page
 * with CLUT 7 of the sprite row, neutral color and scale ONE. */
battle_gfx_sprite_display_data_t* battle_gfx_init_item_sprite_display(s32 runtime_id) {
    battle_gfx_sprite_display_data_t* display;
    s32 i;

    display = &g_battle_units[runtime_id].item_sprite_display.header;
    display->color.b = 0x80;
    display->color.g = 0x80;
    display->color.r = 0x80;
    display->scale_y = ONE;
    display->scale_x = ONE;
    display->tpage = BATTLE_GFX_TPAGE_ITEM;
    display->clut = MAP_CLUT(MAP_CLUT_ROW_SPRITES, 7);
    display->rotation = 0;
    display->part_count = 1;
    for (i = 0; i < 1; i++) {
        display->parts[i].y_shift = 0;
        display->parts[i].x_shift = 0;
        display->parts[i].width = 0x10;
        display->parts[i].height = 0x10;
        display->parts[i].flags = 0;
    }
    return display;
}
