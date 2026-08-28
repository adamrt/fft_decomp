#include "fft/battle.h"
#include "fft/battle_gfx.h"

battle_gfx_sprite_display_data_t* battle_gfx_init_weapon_sprite_display(s32 unit_index, s32 display_type) {
    s32 weapon_part;
    u8* record;
    battle_gfx_sprite_display_data_t* display;
    volatile battle_gfx_sprite_part_display_data_t* part;

    weapon_part = 2;
    record = g_battle_gfx_weapon_sprite_display_data + unit_index * 0x440;
    display = (battle_gfx_sprite_display_data_t*)(record + display_type * 0x24);
    part = (volatile battle_gfx_sprite_part_display_data_t*)display->parts;
    display->blue = 0x80;
    display->green = 0x80;
    display->red = 0x80;
    do {
        part[2].flags = 0;
        weapon_part--;
        part--;
    } while (weapon_part >= 0);
    display->part_count = 0;
    return display;
}
