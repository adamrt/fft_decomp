#include "fft/battle_gfx.h"
#include "psx/types.h"

/* Unpack a unit SEQ file: its two 16-bit header fields (the first sequence of
 * the attack set and the first of the SP2 set) become 32-bit values and its 256
 * sequence offsets pointers into the sequence data, which is appended at
 * g_battle_gfx_seq_data_cursor. An offset of -1 resolves to the start of
 * that data, the first sequence.
 *
 * Near twin of battle_gfx_resolve_packed_offset_table, which resolves the
 * same record without the two 16-bit header fields. As there, the target keeps a
 * second copy of the record pointer for the payload size (`move t2,a1`). */
void battle_gfx_unpack_unit_seq_data(
    battle_gfx_native_unit_seq_table_t* destination, const battle_gfx_packed_seq_record_t* source) {
    const battle_gfx_packed_seq_record_t* header;
    const u8(*entry)[4];
    u32 cursor;
    u32 index;
    u32 payload_size;
    s32 offset;

    header = source;
    cursor = (u32)g_battle_gfx_seq_data_cursor;
    destination->header.first_attack_sequence
        = source->first_attack_sequence[0] + (source->first_attack_sequence[1] << 8);
    destination->header.first_sp2_sequence = source->first_sp2_sequence[0] + (source->first_sp2_sequence[1] << 8);
    entry = source->sequence_offsets;
    for (index = 0; index < 0x100; index++) {
        offset = entry[index][0] + (entry[index][1] << 8) + (entry[index][2] << 16) + (entry[index][3] << 24);
        if (offset == -1) {
            offset = 0;
        }
        destination->sequences[index] = (u8*)(offset + cursor);
    }

    payload_size = header->sequence_data_size[0] + (header->sequence_data_size[1] << 8);
    for (index = 0; index < payload_size; index++) {
        const u8* payload = source->sequence_data;

        g_battle_gfx_seq_data_cursor[index] = payload[index];
    }
    g_battle_gfx_seq_data_cursor += index;
}
