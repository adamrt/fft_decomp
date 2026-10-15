#include "fft/battle_gfx.h"
#include "psx/types.h"

/* Unpack a loaded block of EVTCHR.BIN into an EVTCHR slot whose image
 * buffer is free; returns 0 when it is not.
 *
 * The block is, in order: 0x40 sequence offsets and 0x400 bytes of
 * sequences, 0x28 frame offsets and 0x1e0 bytes of frames, sixteen CLUTs and
 * the 0x6400-byte image. The offsets become pointers into the slot's copies
 * (-1 maps to the start of the data), and the frames are stored twice, once
 * for each of the slot's frame tables.
 *
 * The integer sum with the decayed data array keeps the target's
 * offset-plus-0x108-then-base order; a byte pointer for the palettes keeps
 * that copy an end-pointer loop like the others. */
s32 battle_gfx_unpack_evtchr_file_to_vram_slot(s32 index, const evtchr_file_record_t* record) {
    s32 i;
    s32 pos;
    s32 offset;
    const u8* word;
    u8* dst;
    const u8* file = (const u8*)record;

    if (g_battle_gfx_vram_slots[index].image_buffer_state == BATTLE_GFX_VRAM_SLOT_BUFFER_FREE) {
        /* Separate named field bases fold the retail running byte position. */
        pos = offsetof(evtchr_file_record_t, sequence_offsets);
        for (i = 0; i < 0x40; i++) {
            word = file + pos;
            offset = word[0] + (word[1] << 8) + (word[2] << 16) + (word[3] << 24);
            if (offset == -1) {
                offset = 0;
            }
            g_battle_gfx_vram_slots[index].seq.sequences[i]
                = (u8*)(offset + (s32)g_battle_gfx_vram_slots[index].seq.sequence_data);
            pos += 4;
        }
        for (i = 0; i < 0x400; i++) {
            g_battle_gfx_vram_slots[index].seq.sequence_data[i] = file[pos++];
        }
        for (i = 0; i < 0x28; i++) {
            word = file + pos;
            offset = word[0] + (word[1] << 8) + (word[2] << 16) + (word[3] << 24);
            if (offset == -1) {
                offset = 0;
            }
            g_battle_gfx_vram_slots[index].shp.tables.frames[i]
                = (battle_gfx_source_frame_t*)(offset + (s32)g_battle_gfx_vram_slots[index].shp.data);
            g_battle_gfx_vram_slots[index].shp.tables.submerged_frames[i]
                = (battle_gfx_source_frame_t*)(offset + (s32)g_battle_gfx_vram_slots[index].shp.submerged_data);
            pos += 4;
        }
        for (i = 0; i < 0x1e0; i++) {
            g_battle_gfx_vram_slots[index].shp.data[i] = file[pos];
            g_battle_gfx_vram_slots[index].shp.submerged_data[i] = file[pos];
            pos++;
        }
        dst = (u8*)g_battle_gfx_vram_slots[index].clut;
        for (i = 0; i < 0x200; i++) {
            dst[i] = file[pos++];
        }
        for (i = 0; i < 0x6400; i++) {
            g_battle_gfx_vram_slots[index].image_data[i] = file[pos++];
        }
        g_battle_gfx_vram_slots[index].image_buffer_state = BATTLE_GFX_VRAM_SLOT_BUFFER_EVTCHR;
        g_battle_gfx_vram_slots[index].evtchr_load_state = 3;
        return 1;
    }
    return 0;
}
