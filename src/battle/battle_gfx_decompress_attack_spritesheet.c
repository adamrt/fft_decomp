#include "fft/battle_gfx.h"
#include "psx/types.h"

/* Expands a nibble-RLE attack spritesheet into 0x6400 bytes of 4bpp pixels.
 *
 * A zero nibble introduces a run of transparent pixels whose length follows as
 * one nibble, or as two or three nibbles after the markers 7 and 8. */
void battle_gfx_decompress_attack_spritesheet(u8* source, u8* destination) {
    s32 high;
    s32 count;
    s32 value;
    s32 i;
    s32 size;
    u8* out;

    high = 0;
    count = 0;
    size = 0x6400;
    out = destination;
    g_battle_gfx_compressed_data = source;
    g_battle_gfx_compressed_offset = 0;
    g_battle_gfx_compressed_nibble_phase = 0;
    while (1) {
        value = battle_gfx_read_compressed_nibble();
        if (value == 0) {
            value = battle_gfx_read_compressed_nibble();
            switch (value) {
            case 0:
                value = battle_gfx_read_compressed_nibble();
                break;
            case 7:
                value = battle_gfx_read_compressed_nibble();
                value |= battle_gfx_read_compressed_nibble() << 4;
                break;
            case 8:
                value = battle_gfx_read_compressed_nibble();
                value |= battle_gfx_read_compressed_nibble() << 4;
                value |= battle_gfx_read_compressed_nibble() << 8;
                break;
            }
            for (i = 0; i < value; i++) {
                if (high == 0) {
                    destination[count] = 0;
                } else {
                    out++;
                    destination[count] &= 0xf0;
                    count++;
                    if (count == size) {
                        return;
                    }
                }
                high ^= 1;
            }
        } else {
            if (high == 0) {
                *out = value << 4;
            } else {
                count++;
                *out |= value;
                out++;
                if (count == size) {
                    return;
                }
            }
            high ^= 1;
        }
    }
}
