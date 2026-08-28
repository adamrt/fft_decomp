#include "fft/wldcore.h"
#include "psx/gpu.h"
#include "psx/gs.h"

#define BLOCK_BUFFER ((u8*)0x801EE000)

/*
 * Streams an image into VRAM through the 0x801EE000 staging buffer in 1 KiB
 * bands, copying 32 more bytes of each active band per step in the order
 * given by the g_wldcore_dissolve_band_order offset table.
 *
 * Step 0 clears the buffer to 0xFF, uploads it and resets the per-band
 * progress; it returns no value (the target leaves a pointer in v0). Returns 0
 * once step reaches 50, otherwise 1.
 */
s32 wldcore_gfx_step_dissolve_image_upload(GsIMAGE* image, s32 step) {
    u8* pixels;
    u16* order;
    u8* buffer;
    s32 i;
    s32 j;
    s32 size;
    s32 base;
    s32 index;
    s32 start;
    s32 rows;
    u32 height;
    s32 count;
    s32 first;
    u8* state;

    pixels = (u8*)image->pixel;
    order = g_wldcore_dissolve_band_order;
    buffer = BLOCK_BUFFER;
    if (step == 0) {
        for (i = 0; i < image->pw * (image->ph << 1); i++) {
            buffer[i] = 0xff;
        }
        LoadTPage((u32*)buffer, 0, 0, image->px, image->py, image->pw * 4, image->ph);
        state = &g_wldcore_dissolve_band_progress[15];
        for (i = 15; i >= 0; i--) {
            *state-- = 0;
        }
        return;
    }
    if (step >= 50) {
        return 0;
    }
    step--;
    size = image->pw * image->ph * 2;
    height = image->ph;
    rows = height >> 3;
    if (height & 7) {
        start = 15 - rows;
    } else {
        start = 16 - rows;
    }
    for (i = start; i < 16; i++) {
        if (i <= step && (count = g_wldcore_dissolve_band_progress[i]) < 32U) {
            base = (15 - i) << 10;
            /* Computed before the size check: the target copies it into j. */
            first = count * 32;
            if (base >= size) {
                continue;
            }
            for (j = first; j < first + 32; j++) {
                index = base + order[j];
                if (index < size) {
                    buffer[index] = pixels[index];
                }
            }
            g_wldcore_dissolve_band_progress[i]++;
        }
        LoadTPage((u32*)buffer, 0, 0, image->px, image->py, image->pw * 4, image->ph);
    }
    return 1;
}
