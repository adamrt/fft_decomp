#include "fft/battle_gfx.h"
#include "psx/types.h"

/* Unaligned record: the offset entries and the payload length are stored as
 * little-endian byte sequences, so every field is read one byte at a time. */
typedef struct packed_offset_table {
    u8 unknown_00[4];   /* 0x000 */
    u8 entry[0x100][4]; /* 0x004 */
    u8 payload_size[2]; /* 0x404 */
    u8 payload[1];      /* 0x406 */
} packed_offset_table_t;

/* Resolve the record's 256 packed offsets into blob pointers, then append the
 * record's payload bytes to the blob and advance the allocation cursor.
 *
 * An entry of -1 resolves to the blob start instead of to a real offset.
 *
 * `header` and `unused_frame_pad` are matching constraints. The target keeps
 * two live copies of the record pointer (`move t2,a1` at +0x4, with `a1` still
 * used for the entry base and the payload address), which only a second
 * variable reproduces; and it reserves an 8-byte frame that no instruction
 * touches, which only a declared local aggregate reproduces. */
void battle_gfx_resolve_packed_offset_table(u8** entries, packed_offset_table_t* record) {
    u8(*entry)[4];
    u32 index;
    u32 payload_size;
    u8* payload;
    s32 offset;
    u8 unused_frame_pad[8];
    packed_offset_table_t* header;

    header = record;
    entry = record->entry;
    index = 0;
    do {
        offset = entry[index][0] + (entry[index][1] << 8) + (entry[index][2] << 16) + (entry[index][3] << 24);
        if (offset == -1) {
            offset = 0;
        }
        entries[index] = g_battle_gfx_load_data_cursor + offset;
        index += 1;
    } while (index < 0x100);

    payload_size = header->payload_size[0] + (header->payload_size[1] << 8);
    index = 0;
    if (payload_size != 0) {
        payload = record->payload;
        do {
            g_battle_gfx_load_data_cursor[index] = *payload;
            index += 1;
            payload += 1;
        } while (index < payload_size);
    }
    g_battle_gfx_load_data_cursor += index;
}
