/* SHP/SEQ unpacking and spritesheet, WEP and OTHER image uploads. */

#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
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

/* Unpack a unit SHP file into its two frame tables.
 *
 * The file's first 32-bit value is the offset of its second frame set, the submerged
 * frames; 8 means it has only the set that starts there and both tables get
 * its frames. A set is 0x100 frame offsets, of which the first 0xd0 become
 * pointers into the frame data (-1 maps to its start), then the frame data:
 * a 16-bit count and that many bytes, appended at
 * g_battle_gfx_shp_frame_data_cursor.
 */
void battle_gfx_unpack_unit_shp_data(
    battle_gfx_unit_shp_frame_tables_t* dst, const battle_gfx_packed_unit_shp_record_t* src) {
    u32 i;
    u32 j;
    s32 offset;
    s32 value;
    u32 count;
    const u8* data;
    s32 header;

    /* Named table indexing removes the retail cursor and skip-loop instructions. */
    data = (const u8*)src;
    header = src->submerged_frame_set_offset[0] + (src->submerged_frame_set_offset[1] << 8)
        + (src->submerged_frame_set_offset[2] << 16) + (src->submerged_frame_set_offset[3] << 24);
    dst->first_attack_frame = src->first_attack_frame[0] + (src->first_attack_frame[1] << 8);
    dst->first_sp2_frame = src->first_sp2_frame[0] + (src->first_sp2_frame[1] << 8);
    offset = offsetof(battle_gfx_packed_unit_shp_record_t, primary_frame_set);
    for (i = 0; i < 0xd0; i++) {
        value = data[offset] + (data[offset + 1] << 8) + (data[offset + 2] << 16) + (data[offset + 3] << 24);
        if (value == -1) {
            value = 0;
        }
        dst->frames[i] = (battle_gfx_source_frame_t*)(g_battle_gfx_shp_frame_data_cursor + value);
        if (header == 8) {
            dst->submerged_frames[i] = (battle_gfx_source_frame_t*)(g_battle_gfx_shp_frame_data_cursor + value);
        }
        offset += 4;
    }
    for (i = 0xd0; i < 0x100; i++) {
        offset += 4;
    }
    count = data[offset] + (data[offset + 1] << 8);
    offset += 2;
    for (j = 0; j < count; j++) {
        g_battle_gfx_shp_frame_data_cursor[j] = ((const u8*)src)[offset++];
    }
    g_battle_gfx_shp_frame_data_cursor += j;
    if (header != 8) {
        offset = header;
        for (i = 0; i < 0xd0; i++) {
            value = data[offset] + (data[offset + 1] << 8) + (data[offset + 2] << 16) + (data[offset + 3] << 24);
            if (value == -1) {
                value = 0;
            }
            dst->submerged_frames[i] = (battle_gfx_source_frame_t*)(g_battle_gfx_shp_frame_data_cursor + value);
            offset += 4;
        }
        for (i = 0xd0; i < 0x100; i++) {
            offset += 4;
        }
        count = data[offset] + (data[offset + 1] << 8);
        offset += 2;
        for (j = 0; j < count; j++) {
            g_battle_gfx_shp_frame_data_cursor[j] = ((const u8*)src)[offset++];
        }
        g_battle_gfx_shp_frame_data_cursor += j;
    }
}

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

/* Upload both uncompressed images, copy the compressed attack image,
 * and enable STP on each palette's entries 1..15.
 * Indexed byte copies preserve the original loop scheduling. Three RECT
 * locals preserve the 0x50-byte frame and its otherwise unused x/y stores. */
void battle_gfx_load_spritesheet_into_vram_slot(
    const battle_gfx_unit_sprite_file_t* source, u32 index, s32 unused, battle_unit_t* unit, s16 unused_palette) {
    const u8* data;
    u32 column;
    u8* clut_bytes;
    u8* compressed_attack_image;
    s32 byte_index;
    RECT unused_rect;
    RECT rect;
    RECT unused_rect2;

    /* Byte stores preserve the palette loop and shared image cursor. */
    data = (const u8*)source;
    column = index;
    clut_bytes = (u8*)g_battle_gfx_spritesheet_slots[unit->spritesheet_vram_slot].clut[0];
    compressed_attack_image = g_battle_gfx_spritesheet_slots[unit->spritesheet_vram_slot].compressed_attack_image;
    if (data != 0) {
        for (byte_index = 0; byte_index < 0x200; byte_index++) {
            if (byte_index % 2 != 0 && byte_index % 32 != 1) {
                clut_bytes[byte_index] = data[byte_index] | 0x80;
            } else {
                clut_bytes[byte_index] = data[byte_index];
            }
        }
        data += sizeof(source->clut);
        rect.x = column * 64 + 0x100;
        rect.y = 0x100;
        LoadTPage((u32*)data, 0, 0, rect.x, 0x100, 0x100, 0x100);
        rect.x = (column / 8) * 64 + 0x340;
        rect.y = (column % 8) * 32 + 0x100;
        LoadTPage((u32*)(data + sizeof(source->pixels)), 0, 0, rect.x, rect.y, 0x100, 0x20);
        for (byte_index = 0; byte_index < 0x30d4; byte_index++) {
            compressed_attack_image[byte_index]
                = data[byte_index + sizeof(source->pixels) + sizeof(source->portrait_and_body_pixels)];
        }
    }
}

/* Upload WEP.SPR's weapon, trail and particle images. Keep weapon and trail
 * CLUTs in their CPU tables; the particle CLUTs use the map palette manager. */
void battle_gfx_load_wep_spr_data(battle_gfx_weapon_sprite_archive_t* archive) {
    s32 i;
    u8* cursor;

    cursor = (u8*)archive->weapon_clut;
    for (i = 0; i < (s32)sizeof(archive->weapon_clut); i++) {
        ((u8*)g_battle_gfx_weapon_clut)[i] = *cursor++;
    }
    LoadTPage((u32*)archive->weapon_pixels, 0, 0, 0x240, 0, 0x100, 0x100);
    for (i = 0, cursor = (u8*)archive->trail_clut; i < (s32)sizeof(archive->trail_clut); i++) {
        ((u8*)g_battle_gfx_trail_clut)[i] = *cursor++;
    }
    LoadTPage((u32*)archive->trail_pixels, 0, 0, 0x200, 0, 0x100, 0x100);
    map_load_palette_data(archive->particle_clut, MAP_CLUT_ROW_WEAPONS, 0, 1);
    LoadTPage((u32*)archive->particle_pixels, 0, 0, 0x200, 0x70, 0x100, 0x90);
}

/* Copy the first 21 OTHER.SPR palettes, setting STP on all but color zero. */
void battle_gfx_copy_other_spr_palette_data_to_ram(const battle_gfx_other_sprite_archive_t* archive) {
    s32 i = 0;
    s32 one = 1; /* held in a local: a literal changes the setup schedule */
    u8* dst = (u8*)&g_battle_gfx_other_palettes;
    /* Byte transfers preserve the palette loop and STP store sequence. */
    const u8* src = (const u8*)archive->palettes;
    do {
        u8 color_byte;
        if ((i & 1) && i % 32 != one) {
            color_byte = *src++ | 0x80;
        } else {
            color_byte = *src++;
        }
        *dst++ = color_byte;
        i++;
    } while (i < (s32)sizeof(g_battle_gfx_other_palettes));
}
