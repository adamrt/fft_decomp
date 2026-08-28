#include "fft/battle.h"
#include "fft/battle_gfx.h"

battle_gfx_sprite_display_data_t* battle_gfx_init_item_sprite_display(s32 unit_index) {
    s32 part_size;
    s32 count;
    battle_gfx_sprite_display_data_t* display;
    /* volatile stops GCC strength-reducing the record walk into a pointer
     * biased by +0x14 with negative displacements. */
    volatile battle_gfx_sprite_part_display_data_t* part;

    count = 0;
    part_size = 0x10;
    display = (battle_gfx_sprite_display_data_t*)&g_battle_gfx_item_sprite_display_data[unit_index * 0x440];
    /* The target walks the part records from the slot base (parts[0] sits
     * two record-sizes in), so index by 2 rather than starting at ->parts. */
    part = (volatile battle_gfx_sprite_part_display_data_t*)display;
    display->blue = 0x80;
    display->green = 0x80;
    display->red = 0x80;
    display->scale_y = ONE;
    display->scale_x = ONE;
    display->spritesheet_id = BATTLE_SPRITESHEET_ID_ITEM;
    display->clut = 0x7887;
    display->y_rotation = 0;
    display->part_count = 1;
    do {
        part[2].y_shift = 0;
        part[2].x_shift = 0;
        part[2].width = part_size;
        part[2].height = part_size;
        part[2].flags = 0;
        part++;
        count++;
    } while (count <= 0);
    return display;
}
