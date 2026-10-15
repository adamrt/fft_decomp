#include "fft/battle_gfx.h"
#include "psx/types.h"

/* Unpack a weapon or effect SEQ file, which has no header fields: its 256
 * sequence offsets become pointers into the sequence data, which is appended
 * at g_battle_gfx_seq_data_cursor.
 *
 * An offset of -1 resolves to the start of that data instead of to a real
 * sequence.
 *
 * `header` and `unused_frame_pad` are matching constraints. The target keeps
 * two live copies of the record pointer (`move t2,a1` at +0x4, with `a1` still
 * used for the entry base and the payload address), which only a second
 * variable reproduces; and it reserves an 8-byte frame that no instruction
 * touches, which only a declared local aggregate reproduces. */
void battle_gfx_resolve_packed_offset_table(u8** entries, const battle_gfx_packed_offset_table_t* record) {
    const u8(*entry)[4];
    u32 index;
    u32 payload_size;
    const u8* payload;
    s32 offset;
    u8 unused_frame_pad[8];
    const battle_gfx_packed_offset_table_t* header;

    header = record;
    entry = record->entry;
    index = 0;
    do {
        offset = entry[index][0] + (entry[index][1] << 8) + (entry[index][2] << 16) + (entry[index][3] << 24);
        if (offset == -1) {
            offset = 0;
        }
        entries[index] = g_battle_gfx_seq_data_cursor + offset;
        index += 1;
    } while (index < 0x100);

    payload_size = header->payload_size[0] + (header->payload_size[1] << 8);
    index = 0;
    if (payload_size != 0) {
        payload = record->payload;
        do {
            g_battle_gfx_seq_data_cursor[index] = *payload;
            index += 1;
            payload += 1;
        } while (index < payload_size);
    }
    g_battle_gfx_seq_data_cursor += index;
}
