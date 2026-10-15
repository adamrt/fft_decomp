#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "psx/gte_inline.h"
#include "psx/gte_inline_split.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Reset the display of one of a unit's three sprite blocks: neutral color,
 * no flags on its three parts and none of them in use. */
battle_gfx_sprite_display_data_t* battle_gfx_init_weapon_sprite_display(s32 unit_index, s32 display_type) {
    s32 weapon_part;
    battle_gfx_sprite_display_data_t* display;

    display = &g_battle_units[unit_index].weapon_sprite_displays[display_type].header;
    display->color.b = 0x80;
    display->color.g = 0x80;
    display->color.r = 0x80;
    for (weapon_part = 2; weapon_part >= 0; weapon_part--) {
        display->parts[weapon_part].flags = 0;
    }
    display->part_count = 0;
    return display;
}
