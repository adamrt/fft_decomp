#include "fft/battle.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Unpack a loaded EVTCHR/SP2 file into a free VRAM cache slot.
 *
 * Returns 0 when the slot already holds data. The file's little-endian frame
 * offsets become pointers into the slot's copied frame data (-1 maps to the
 * start of the data); the frame data, palettes and 0x6400-byte image follow
 * in file order. The second section's frame data is stored twice, once for
 * each frame table.
 *
 * The integer sum with the decayed data array keeps the target's
 * offset-plus-0x108-then-base order; a byte pointer for the palettes keeps
 * that copy an end-pointer loop like the others. */
s32 battle_gfx_unpack_evtchr_file_to_vram_slot(s32 index, u8* file) {
    s32 i;
    s32 pos;
    s32 offset;
    u8* word;
    u8* dst;

    if (g_battle_gfx_vram_slots[index].evtchr_load_marker == 0) {
        pos = 0;
        for (i = 0; i < 0x40; i++) {
            word = file + pos;
            offset = word[0] + (word[1] << 8) + (word[2] << 16) + (word[3] << 24);
            if (offset == -1) {
                offset = 0;
            }
            g_battle_gfx_vram_slots[index].shp0.frames[i]
                = (u8*)(offset + (s32)g_battle_gfx_vram_slots[index].shp0.data);
            pos += 4;
        }
        for (i = 0; i < 0x400; i++) {
            g_battle_gfx_vram_slots[index].shp0.data[i] = file[pos++];
        }
        for (i = 0; i < 0x28; i++) {
            word = file + pos;
            offset = word[0] + (word[1] << 8) + (word[2] << 16) + (word[3] << 24);
            if (offset == -1) {
                offset = 0;
            }
            g_battle_gfx_vram_slots[index].shp1.frames[i]
                = (u8*)(offset + (s32)g_battle_gfx_vram_slots[index].shp1.data);
            g_battle_gfx_vram_slots[index].shp1.submerged_frames[i]
                = (u8*)(offset + (s32)g_battle_gfx_vram_slots[index].shp1.submerged_data);
            pos += 4;
        }
        for (i = 0; i < 0x1e0; i++) {
            g_battle_gfx_vram_slots[index].shp1.data[i] = file[pos];
            g_battle_gfx_vram_slots[index].shp1.submerged_data[i] = file[pos];
            pos++;
        }
        dst = (u8*)g_battle_gfx_vram_slots[index].cluts;
        for (i = 0; i < 0x200; i++) {
            dst[i] = file[pos++];
        }
        for (i = 0; i < 0x6400; i++) {
            g_battle_gfx_vram_slots[index].image_data[i] = file[pos++];
        }
        g_battle_gfx_vram_slots[index].evtchr_load_marker = 0xfe;
        g_battle_gfx_vram_slots[index].evtchr_load_state = 3;
        return 1;
    }
    return 0;
}
