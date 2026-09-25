#include "fft/battle.h"
#include "psx/types.h"

typedef struct battle_gfx_shp_frame_table {
    s16 header[32];
    u8* entries[0x200];
} battle_gfx_shp_frame_table_t;

/*
 * Unpack a little-endian resource into dst: 32 halfword header entries, 0x200
 * word offsets rebased onto the g_battle_gfx_shp_frame_data_cursor bump buffer (-1 maps to offset 0),
 * then a halfword-counted byte blob appended to that buffer.
 *
 * The header loops read through `data` while the blob copy indexes `src`;
 * using one name for both loses the target's separate copy of the argument.
 */
void battle_gfx_copy_shp_data_to_ram(battle_gfx_shp_frame_table_t* dst, u8* src) {
    u32 i;
    u32 j;
    s32 offset;
    s32 value;
    u32 count;
    u8* word;
    u8* data;

    data = src;
    offset = 4;
    for (i = 0; i < 32; i++) {
        word = data + offset;
        dst->header[i] = word[0] | (word[1] << 8);
        offset += 2;
    }
    for (i = 0; i < 0x200; i++) {
        word = data + offset;
        value = word[0] + (word[1] << 8) + (word[2] << 16) + (word[3] << 24);
        if (value == -1) {
            value = 0;
        }
        dst->entries[i] = g_battle_gfx_shp_frame_data_cursor + value;
        offset += 4;
    }
    word = data + offset;
    offset += 2;
    count = word[0] + (word[1] << 8);
    for (j = 0; j < count; j++) {
        g_battle_gfx_shp_frame_data_cursor[j] = src[offset++];
    }
    g_battle_gfx_shp_frame_data_cursor += j;
}
