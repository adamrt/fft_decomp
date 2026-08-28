#include "psx/types.h"

void battle_gfx_set_sprite_part_flag_bits_1_2(u8* display, s32 part_index, u32 flags) {
    u8* part;

    part = display + part_index * 7;
    part[0x14] = (part[0x14] & 0xf9) | flags;
}
