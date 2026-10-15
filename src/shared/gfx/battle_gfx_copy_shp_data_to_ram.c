#include "fft/battle_gfx.h"
#include "psx/types.h"

/*
 * Unpack a weapon or effect SHP file into dst: after four unread bytes, the
 * first frame of each of 32 weapon types, 0x200 frame offsets that become
 * pointers into the frame data (-1 maps to its start), and the frame data
 * itself, a 16-bit count and that many bytes, which is appended at
 * g_battle_gfx_shp_frame_data_cursor.
 *
 * The header loops read through `data` while the blob copy indexes `src`;
 * using one name for both loses the target's separate copy of the argument.
 */
void battle_gfx_copy_shp_data_to_ram(battle_gfx_weapon_shp_t* dst, const battle_gfx_packed_weapon_shp_record_t* src) {
    u32 i;
    u32 j;
    s32 offset;
    s32 value;
    u32 count;
    const u8* word;
    const u8* data;

    /* Named field indexing removes the retail running-offset instructions. */
    data = (const u8*)src;
    offset = offsetof(battle_gfx_packed_weapon_shp_record_t, first_frame);
    for (i = 0; i < 32; i++) {
        word = data + offset;
        dst->first_frame[i] = word[0] | (word[1] << 8);
        offset += 2;
    }
    for (i = 0; i < 0x200; i++) {
        word = data + offset;
        value = word[0] + (word[1] << 8) + (word[2] << 16) + (word[3] << 24);
        if (value == -1) {
            value = 0;
        }
        dst->frames[i] = (battle_gfx_source_frame_t*)(g_battle_gfx_shp_frame_data_cursor + value);
        offset += 4;
    }
    word = data + offset;
    offset += 2;
    count = word[0] + (word[1] << 8);
    for (j = 0; j < count; j++) {
        g_battle_gfx_shp_frame_data_cursor[j] = ((const u8*)src)[offset++];
    }
    g_battle_gfx_shp_frame_data_cursor += j;
}
