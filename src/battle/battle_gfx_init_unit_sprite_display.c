#include "fft/battle_gfx.h"
#include "psx/types.h"

/* Reset a unit's own sprite display: neutral color, no flags on its eight
 * parts and none of them in use. */
battle_gfx_sprite_display_data_t* battle_gfx_init_unit_sprite_display(s32 runtime_id) {
    s32 body_part;
    battle_gfx_sprite_display_data_t* display;

    display = &g_battle_units[runtime_id].unit_sprite_display.header;
    display->color.b = 0x80;
    display->color.g = 0x80;
    display->color.r = 0x80;
    for (body_part = 7; body_part >= 0; body_part--) {
        display->parts[body_part].flags = 0;
    }
    display->part_count = 0;
    return display;
}
