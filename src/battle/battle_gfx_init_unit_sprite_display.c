#include "fft/battle.h"
#include "psx/types.h"

u8* battle_gfx_init_unit_sprite_display(s32 unit_index) {
    s32 body_part;
    u8* display;
    u8* flags;

    body_part = 7;
    display = &g_battle_gfx_unit_sprite_display_data[unit_index * 0x440];
    flags = display + 0x31;
    display[2] = 0x80;
    display[1] = 0x80;
    display[0] = 0x80;
    do {
        flags[0x14] = 0;
        body_part--;
        flags -= 7;
    } while (body_part >= 0);
    display[3] = 0;
    return display;
}
