#include "fft/battle.h"
#include "psx/types.h"

/* Decode a two-set little-endian resource into the destination pointer
 * tables and append its counted blobs to the g_battle_gfx_shp_frame_data_cursor bump buffer.
 *
 * A header of 8 repeats the primary table.
 */
void battle_gfx_unpack_unit_shp_data(battle_gfx_unit_shp_frame_tables_t* dst, u8* src) {
    u32 i;
    u32 j;
    s32 offset;
    s32 value;
    u32 count;
    u8* data;
    s32 header;

    data = src;
    header = data[0] + (data[1] << 8) + (data[2] << 16) + (data[3] << 24);
    dst->attack_frame_start = data[4] + (data[5] << 8);
    dst->sp2_frame_start = data[6] + (data[7] << 8);
    offset = 8;
    for (i = 0; i < 0xd0; i++) {
        value = data[offset] + (data[offset + 1] << 8) + (data[offset + 2] << 16) + (data[offset + 3] << 24);
        if (value == -1) {
            value = 0;
        }
        dst->primary[i] = g_battle_gfx_shp_frame_data_cursor + value;
        if (header == 8) {
            dst->secondary[i] = g_battle_gfx_shp_frame_data_cursor + value;
        }
        offset += 4;
    }
    for (i = 0xd0; i < 0x100; i++) {
        offset += 4;
    }
    count = data[offset] + (data[offset + 1] << 8);
    offset += 2;
    for (j = 0; j < count; j++) {
        g_battle_gfx_shp_frame_data_cursor[j] = src[offset++];
    }
    g_battle_gfx_shp_frame_data_cursor += j;
    if (header != 8) {
        offset = header;
        for (i = 0; i < 0xd0; i++) {
            value = data[offset] + (data[offset + 1] << 8) + (data[offset + 2] << 16) + (data[offset + 3] << 24);
            if (value == -1) {
                value = 0;
            }
            dst->secondary[i] = g_battle_gfx_shp_frame_data_cursor + value;
            offset += 4;
        }
        for (i = 0xd0; i < 0x100; i++) {
            offset += 4;
        }
        count = data[offset] + (data[offset + 1] << 8);
        offset += 2;
        for (j = 0; j < count; j++) {
            g_battle_gfx_shp_frame_data_cursor[j] = src[offset++];
        }
        g_battle_gfx_shp_frame_data_cursor += j;
    }
}
